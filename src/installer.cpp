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

extern "C" {
#include "zip.h"
}

namespace fs = std::filesystem;

namespace muuk {
    namespace package_manager {

        void clone_shallow_repo(const std::string& repo_url, const std::string& target_dir, const std::string& checkout_ref) {
            if (fs::exists(target_dir)) {
                logger_->info("Repository already exists. Removing old version...");
                fs::remove_all(target_dir);
            }

            std::string clone_cmd = "git clone --depth=1 --single-branch";

            // Clone with the specified branch if available
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                clone_cmd += " --branch " + checkout_ref;
            }

            clone_cmd += " " + repo_url + " " + target_dir;

            logger_->info("Cloning repository: {}", clone_cmd);
            int clone_result = util::execute_command(clone_cmd);
            if (clone_result != 0) {
                logger_->error("Failed to clone repository '{}'", repo_url);
                return;
            }

            // If a specific commit (revision) or tag is provided, check it out
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                std::string checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                logger_->info("Checking out reference: {}", checkout_ref);
                int checkout_result = util::execute_command(checkout_cmd);
                if (checkout_result != 0) {
                    logger_->error("Failed to checkout reference '{}' in '{}'", checkout_ref, target_dir);
                    return;
                }
            }

            // Remove the .git folder to keep things clean
            std::string git_folder = target_dir + "/.git";
            if (fs::exists(git_folder)) {
                logger_->info("Removing .git folder to keep it clean.");
                fs::remove_all(git_folder);
            }
        }

        void generate_default_muuk_toml(
            const std::string& repo_path,
            const std::string& repo_name,
            const std::string& version,
            const std::string& revision,
            const std::string& tag
        ) {
            std::string muuk_toml_path = repo_path + "/muuk.toml";

            try {
                std::ofstream muuk_toml(muuk_toml_path);
                if (!muuk_toml) {
                    logger_->error("Failed to create muuk.toml at '{}'", muuk_toml_path);
                    return;
                }

                logger_->info("Creating default muuk.toml for '{}'", repo_name);

                // Determine which versioning key to use
                std::string version_key = "version";
                std::string version_value = version;

                if (!revision.empty()) {
                    version_key = "revision";
                    version_value = revision;
                }
                else if (!tag.empty()) {
                    version_key = "tag";
                    version_value = tag;
                }

                // Write default muuk.toml
                muuk_toml << "[package]\n";
                muuk_toml << "name = \"" << repo_name << "\"\n";
                muuk_toml << version_key << " = \"" << version_value << "\"\n\n";

                muuk_toml << "[library." << repo_name << "]\n";
                muuk_toml << "include = [\"include/\"]\n";
                muuk_toml << "sources = [\"src/*.cpp\", \"src/*.cc\"]\n";

                muuk_toml.close();
                logger_->info("Generated default muuk.toml at '{}'", muuk_toml_path);
            }
            catch (const std::exception& e) {
                logger_->error("Error writing muuk.toml for '{}': {}", repo_name, e.what());
            }
        }

        bool is_installed_version_matching(const std::string& muuk_toml_path,
            const std::string& version, const std::string& revision, const std::string& tag) {
            try {
                MuukFiler muukFiler(muuk_toml_path);
                toml::table config = muukFiler.get_config();

                if (!config.contains("package") || !config["package"].is_table()) {
                    logger::warning("'{}' has no valid [package] section.", muuk_toml_path);
                    return false;
                }

                toml::table package_section = *config["package"].as_table();

                std::string installed_version = package_section.contains("version") ? *package_section["version"].value<std::string>() : "";
                std::string installed_revision = package_section.contains("revision") ? *package_section["revision"].value<std::string>() : "";
                std::string installed_tag = package_section.contains("tag") ? *package_section["tag"].value<std::string>() : "";

                return (installed_version == version || installed_revision == revision || installed_tag == tag);
            }
            catch (const std::exception& e) {
                logger_->error("Error checking installed version: {}", e.what());
                return false;
            }
        }

        void install(const std::string& lockfile_path = "muuk.lock.toml") {
            MuukLockGenerator lockgen("./");
            lockgen.generate_lockfile(lockfile_path);

            logger_->info("Reading dependencies from '{}'", lockfile_path);

            if (!fs::exists(lockfile_path)) {
                logger_->error("muuk.lock.toml not found.");
                throw std::runtime_error("muuk.lock.toml file not found.");
            }

            MuukFiler muukFiler(lockfile_path);
            toml::table lockfile_data = muukFiler.get_config();

            if (!lockfile_data.contains("dependencies") || !lockfile_data["dependencies"].is_table()) {
                logger_->error("No 'dependencies' section found in '{}'", lockfile_path);
                return;
            }

            toml::table dependencies = *lockfile_data["dependencies"].as_table();

            if (dependencies.empty()) {
                logger_->info("No dependencies found in '{}'", lockfile_path);
                return;
            }

            for (const auto& [dep_name_, dep_table] : dependencies) {
                std::string repo_name = std::string(dep_name_.str());

                if (!dep_table.is_table()) {
                    logger::warning("Skipping invalid dependency entry '{}'", repo_name);
                    continue;
                }

                auto dep_info = *dep_table.as_table();
                std::string git_url = dep_info.contains("git") ? *dep_info["git"].value<std::string>() : "";
                std::string version = dep_info.contains("version") ? *dep_info["version"].value<std::string>() : "latest";
                std::string revision = dep_info.contains("revision") ? *dep_info["revision"].value<std::string>() : "";
                std::string tag = dep_info.contains("tag") ? *dep_info["tag"].value<std::string>() : "";
                std::string branch = dep_info.contains("branch") ? *dep_info["branch"].value<std::string>() : "";

                if (git_url.empty()) {
                    logger::warning("Dependency '{}' has no Git URL. Skipping.", repo_name);
                    continue;
                }

                std::string ref = version.empty() ? (revision.empty() ? branch : revision) : version;
                logger_->info("Installing '{}': ref={}, git={}", repo_name, ref, git_url);

                std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name;
                std::string muuk_toml_path = target_dir + "/muuk.toml";

                if (fs::exists(muuk_toml_path)) {
                    if (is_installed_version_matching(muuk_toml_path, version, revision, tag)) {
                        logger_->info("'{}' is already up to date. Skipping installation.", repo_name);
                        continue;
                    }
                    logger::warning("'{}' version mismatch. Reinstalling...", repo_name);
                    fs::remove_all(target_dir); // Remove outdated version
                }

                if (fs::exists(target_dir)) {
                    for (const auto& entry : fs::directory_iterator(target_dir)) {
                        if (entry.path() != muuk_toml_path) {
                            fs::remove_all(entry.path());
                        }
                    }
                }

                logger_->info("Installing '{}': ref={}, git={}", repo_name, version, git_url);
                clone_shallow_repo(git_url, target_dir, ref);

                if (!fs::exists(muuk_toml_path)) {
                    logger::warning("No muuk.toml found for '{}'. Generating a default one.", repo_name);
                    generate_default_muuk_toml(target_dir, repo_name, version, revision, tag);
                }
            }

            logger_->info("Finished installing all dependencies.");
        }

        void remove_package(const std::string& package_name, const std::string& toml_path, const std::string& lockfile_path) {
            logger_->info("Removing package: {}", package_name);

            try {
                if (!fs::exists(toml_path)) {
                    logger_->error("muuk.toml file not found at '{}'", toml_path);
                    throw std::runtime_error("muuk.toml file not found.");
                }

                MuukFiler muukFiler(toml_path);
                toml::table& dependencies = muukFiler.get_section("dependencies");

                if (!dependencies.contains(package_name)) {
                    logger_->error("Package '{}' is not listed in '{}'", package_name, toml_path);
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

                std::string package_path = std::string(DEPENDENCY_FOLDER) + "/" + package_name;
                if (fs::exists(package_path)) {
                    fs::remove_all(package_path);
                    logger_->info("Removed package directory: {}", package_path);
                }
                else {
                    logger::warning("Package directory '{}' does not exist. Skipping.", package_path);
                }

                logger_->info("Successfully removed package '{}'", package_name);
            }
            catch (const std::exception& e) {
                logger_->error("Error removing package '{}': {}", package_name, e.what());
                throw;
            }
        }
    }
}