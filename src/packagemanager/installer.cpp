#include "logger.h"
#include "buildconfig.h"
#include "util.h"
#include "package_manager.h"
#include "muukfiler.h"
#include "muuklockgen.h"

#include <string>
#include <format>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <toml++/toml.hpp>
#include <tl/expected.hpp>

extern "C" {
#include "zip.h"
}

namespace fs = std::filesystem;

namespace muuk {
    namespace package_manager {

        void clone_shallow_repo(const std::string& repo_url, const std::string& target_dir, const std::string& checkout_ref) {
            fs::path muuk_toml_path = target_dir + "/muuk.toml";
            fs::path parent_repo_dir = fs::path(target_dir).parent_path(); // Get parent directory
            fs::path temp_muuk_toml_path = parent_repo_dir / "_muuk_backup.toml";

            muuk::logger::info("Cloning repository: {} in target dir {}", repo_url, target_dir);
            // Move `muuk.toml` to the parent directory before deleting the old repository
            if (fs::exists(muuk_toml_path)) {
                muuk::logger::info("Moving 'muuk.toml' to parent directory '{}' before removing old repository.", parent_repo_dir.string());
                fs::rename(muuk_toml_path, temp_muuk_toml_path);
            }

            // Remove entire target directory
            if (fs::exists(target_dir)) {
                muuk::logger::info("Repository already exists. Removing old version...");
                fs::remove_all(target_dir);
            }

            std::string clone_cmd = "git clone --depth=1 --single-branch";

            // Clone with the specified branch if available
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                clone_cmd += " --branch " + checkout_ref;
            }

            clone_cmd += " " + repo_url + " " + target_dir;

            muuk::logger::info("Cloning repository: {}", clone_cmd);
            int clone_result = util::execute_command(clone_cmd);
            if (clone_result != 0) {
                muuk::logger logger_instance;
                logger_instance.error("Failed to clone repository '{}'", repo_url);
                return;
            }

            // If a specific commit (revision) or tag is provided, check it out
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                std::string checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                muuk::logger::info("Checking out reference: {}", checkout_ref);
                int checkout_result = util::execute_command(checkout_cmd);
                if (checkout_result != 0) {
                    muuk::logger::error("Failed to checkout reference '{}' in '{}'", checkout_ref, target_dir);
                    return;
                }
            }

            // Move `muuk.toml` back from the parent directory to the newly cloned repository
            if (fs::exists(temp_muuk_toml_path)) {
                muuk::logger::info("Restoring 'muuk.toml' from parent directory.");
                fs::rename(temp_muuk_toml_path, muuk_toml_path);
            }

            // Remove the .git folder to keep things clean
            std::string git_folder = target_dir + "/.git";
            if (fs::exists(git_folder)) {
                muuk::logger::info("Removing .git folder to keep it clean.");
                fs::remove_all(git_folder);
            }
        }


        bool is_installed_version_matching(const fs::path muuk_toml_path, const std::string& version, const std::string& revision, const std::string& tag) {
            try {
                auto muukFiler = MuukFiler::create(muuk_toml_path.string());
                if (!muukFiler) {
                    muuk::logger::error("Error reading '{}'", muuk_toml_path.string());
                    return false;
                }

                toml::table config = muukFiler->get_config();

                if (!config.contains("package") || !config["package"].is_table()) {
                    muuk::logger::warn("'{}' has no valid [package] section.", muuk_toml_path.string());
                    return false;
                }

                toml::table package_section = *config["package"].as_table();

                std::string installed_version = package_section.contains("version") ? *package_section["version"].value<std::string>() : "";
                std::string installed_revision = package_section.contains("revision") ? *package_section["revision"].value<std::string>() : "";
                std::string installed_tag = package_section.contains("tag") ? *package_section["tag"].value<std::string>() : "";

                muuk::logger::debug("Version: {}, revision: {}, tag: {}", version, revision, tag);
                muuk::logger::debug("Installed version: {}, revision: {}, tag: {}", installed_version, installed_revision, installed_tag);

                if (installed_version.empty() && installed_revision.empty() && installed_tag.empty()) {
                    muuk::logger::warn("No versioning information found in '{}'. Assuming outdated.", muuk_toml_path.string());
                    return false;
                }

                return (installed_version == version || installed_revision == revision || installed_tag == tag);
            }
            catch (const std::exception& e) {
                muuk::logger::error("Error checking installed version: {}", e.what());
                return false;
            }
        }

        tl::expected<void, std::string> install(const std::string& lockfile_path_string) {
            fs::path lockfile_path = fs::path(lockfile_path_string);
            MuukLockGenerator lockgen("./");
            lockgen.generate_lockfile(lockfile_path.string());

            muuk::logger::info("Reading dependencies from '{}'", lockfile_path.string());

            if (!fs::exists(lockfile_path)) {
                return tl::unexpected("muuk.lock.toml file not found.");
            }

            MuukFiler muukFiler(lockfile_path.string());
            toml::table lockfile_data = muukFiler.get_config();

            if (!lockfile_data.contains("dependencies") || !lockfile_data["dependencies"].is_table()) {
                return tl::unexpected("No 'dependencies' section found in '" + lockfile_path.string() + "'");
            }

            toml::table dependencies = *lockfile_data["dependencies"].as_table();

            if (dependencies.empty()) {
                muuk::logger::info("No dependencies found in '{}'", lockfile_path.string());
                return {};
            }

            for (const auto& [dep_name_, dep_table] : dependencies) {
                std::string repo_name = std::string(dep_name_.str());

                if (!dep_table.is_table()) {
                    muuk::logger::warn("Skipping invalid dependency entry '{}'", repo_name);
                    continue;
                }

                auto dep_info = *dep_table.as_table();
                std::string git_url = dep_info.contains("git") ? *dep_info["git"].value<std::string>() : "";
                std::string version = dep_info.contains("version") ? *dep_info["version"].value<std::string>() : "latest";
                std::string revision = dep_info.contains("revision") ? *dep_info["revision"].value<std::string>() : "";
                std::string tag = dep_info.contains("tag") ? *dep_info["tag"].value<std::string>() : "";
                std::string branch = dep_info.contains("branch") ? *dep_info["branch"].value<std::string>() : "";

                if (git_url.empty()) {
                    muuk::logger::warn("Dependency '{}' has no Git URL. Skipping.", repo_name);
                    continue;
                }

                std::string ref = version.empty() ? (revision.empty() ? branch : revision) : version;

                if (ref.empty()) {
                    muuk::logger::warn("No version, revision, or branch specified for '{}'. Defaulting to 'main'.", repo_name);
                    ref = "main";  // Fallback to 'main' if nothing is specified
                }

                muuk::logger::info("Installing '{}': ref={}, git={}", repo_name, ref, git_url);

                std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name;
                fs::path muuk_toml_path = target_dir + "/muuk.toml";

                if (fs::exists(muuk_toml_path)) {
                    if (is_installed_version_matching(muuk_toml_path, version, revision, tag)) {
                        muuk::logger::info("'{}' is already up to date. Skipping installation.", repo_name);
                        continue;
                    }
                    muuk::logger::warn("'{}' version mismatch. Reinstalling...", repo_name);
                    // fs::remove_all(target_dir); // Remove outdated version
                }

                muuk::logger::info("Installing '{}': ref={}, git={}", repo_name, version, git_url);
                clone_shallow_repo(git_url, target_dir, ref);

                if (fs::exists(muuk_toml_path)) {
                    muuk::logger::info("Updating '{}' with installed version information.", muuk_toml_path.string());
                    MuukFiler muukFilerInstalled(muuk_toml_path.string());
                    toml::table& package_section = muukFilerInstalled.get_section("package");

                    if (!version.empty()) package_section.insert_or_assign("version", version);
                    if (!revision.empty()) package_section.insert_or_assign("revision", revision);
                    if (!tag.empty()) package_section.insert_or_assign("tag", tag);

                    muukFiler.write_to_file();
                }
                else {
                    muuk::logger::warn("Failed to install '{}'.", repo_name);
                }
            }

            muuk::logger::info("Finished installing all dependencies.");
            return {};
        }

        void remove_package(const std::string& package_name, const std::string& toml_path, const std::string& lockfile_path) {
            muuk::logger::info("Removing package: {}", package_name);

            try {
                if (!fs::exists(toml_path)) {
                    muuk::logger::error("muuk.toml file not found at '{}'", toml_path);
                    throw std::runtime_error("muuk.toml file not found.");
                }

                MuukFiler muukFiler(toml_path);
                toml::table& dependencies = muukFiler.get_section("dependencies");

                if (!dependencies.contains(package_name)) {
                    muuk::logger::error("Package '{}' is not listed in '{}'", package_name, toml_path);
                    throw std::runtime_error("Package not found in muuk.toml.");
                }

                dependencies.erase(package_name);
                muukFiler.write_to_file();

                if (fs::exists(lockfile_path)) {
                    MuukFiler lockFiler(lockfile_path);
                    toml::table& lock_dependencies = lockFiler.get_section("dependencies");

                    if (lock_dependencies.contains(package_name)) {
                        lock_dependencies.erase(package_name);
                        lockFiler.write_to_file();
                    }
                }

                fs::path package_path = std::string(DEPENDENCY_FOLDER) + "/" + package_name;
                if (fs::exists(package_path)) {
                    fs::remove_all(package_path);
                    muuk::logger::info("Removed package directory: {}", package_path.string());
                }
                else {
                    muuk::logger::warn("Package directory '{}' does not exist. Skipping.", package_path.string());
                }

                muuk::logger::info("Successfully removed package '{}'", package_name);
            }
            catch (const std::exception& e) {
                muuk::logger::error("Error removing package '{}': {}", package_name, e.what());
                throw;
            }
        }
    }
}