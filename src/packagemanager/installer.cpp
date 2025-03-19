#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "buildconfig.h"
#include "logger.h"
#include "muukfiler.h"
#include "muuklockgen.h"
#include "package_manager.h"
#include "util.h"

namespace fs = std::filesystem;
namespace muuk {
    namespace package_manager {

        constexpr std::string HASH_FILE_NAME = ".muuk.hash";

        // Determines if the package is installed by checking if `.muuk.hash` exists
        bool is_package_installed(const fs::path& target_dir) {
            return fs::exists(target_dir / HASH_FILE_NAME);
        }

        void create_hash_file(const fs::path& target_dir) {
            std::ofstream file(target_dir / HASH_FILE_NAME);
            if (file.is_open()) {
                file << "installed";
                file.close();
            }
        }

        void delete_hash_file(const fs::path& target_dir) {
            fs::path hash_file = target_dir / HASH_FILE_NAME;
            if (fs::exists(hash_file)) {
                fs::remove(hash_file);
            }
        }

        void clone_shallow_repo(const std::string& repo_url, const std::string& target_dir, const std::string& checkout_ref) {
            fs::path muuk_toml_path = target_dir + "/muuk.toml";
            fs::path parent_repo_dir = fs::path(target_dir).parent_path();
            fs::path temp_muuk_toml_path = parent_repo_dir / "_muuk_backup.toml";

            muuk::logger::info("Cloning repository: {} in target dir {}", repo_url, target_dir);

            if (fs::exists(muuk_toml_path)) {
                muuk::logger::info("Moving 'muuk.toml' to parent directory '{}' before removing old repository.", parent_repo_dir.string());
                fs::rename(muuk_toml_path, temp_muuk_toml_path);
            }

            if (fs::exists(target_dir)) {
                muuk::logger::info("Repository already exists. Removing old version...");
                fs::remove_all(target_dir);
            }

            std::string clone_cmd = "git clone --depth=1 --single-branch";
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                clone_cmd += " --branch " + checkout_ref;
            }
            clone_cmd += " " + repo_url + " " + target_dir;

            muuk::logger::info("Cloning repository: {}", clone_cmd);
            int clone_result = util::command_line::execute_command(clone_cmd);
            if (clone_result != 0) {
                muuk::logger::error("Failed to clone repository '{}'", repo_url);
                return;
            }

            if (!checkout_ref.empty() && checkout_ref != "latest") {
                std::string checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                muuk::logger::info("Checking out reference: {}", checkout_ref);
                int checkout_result = util::command_line::execute_command(checkout_cmd);
                if (checkout_result != 0) {
                    muuk::logger::error("Failed to checkout reference '{}' in '{}'", checkout_ref, target_dir);
                    return;
                }
            }

            if (fs::exists(temp_muuk_toml_path)) {
                muuk::logger::info("Restoring 'muuk.toml' from parent directory.");
                fs::rename(temp_muuk_toml_path, muuk_toml_path);
            }

            std::string git_folder = target_dir + "/.git";
            if (fs::exists(git_folder)) {
                muuk::logger::info("Removing .git folder to keep it clean.");
                fs::remove_all(git_folder);
            }

            create_hash_file(target_dir);
        }

        tl::expected<void, std::string> install(const std::string& lockfile_path_string) {
            fs::path lockfile_path = fs::path(lockfile_path_string);
            MuukLockGenerator lockgen("./");
            lockgen.generate_lockfile(lockfile_path.string());

            muuk::logger::info("Reading dependencies from '{}'", lockfile_path.string());

            if (!fs::exists(lockfile_path)) {
                return tl::unexpected("muuk.lock.toml file not found.");
            }

            auto result = MuukFiler::create(lockfile_path.string(), true);
            if (!result) {
                return Err("Failed to read lockfile: {}", result.error());
            }

            MuukFiler muuk_filer = result.value();
            toml::table lockfile_data = muuk_filer.get_config();

            if (!lockfile_data.contains("dependencies") || !lockfile_data["dependencies"].is_table()) {
                return tl::unexpected("No 'dependencies' section found in '" + lockfile_path.string() + "'");
            }

            toml::table dependencies = *lockfile_data["dependencies"].as_table();

            for (const auto& [dep_name_, dep_table] : dependencies) {
                std::string repo_name = std::string(dep_name_.str());
                auto dep_info = *dep_table.as_table();
                std::string git_url = dep_info.contains("git") ? *dep_info["git"].value<std::string>() : "";
                std::string ref = dep_info.contains("version") ? *dep_info["version"].value<std::string>() : "main";

                std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name + '/' + ref;

                if (fs::exists(target_dir) && is_package_installed(target_dir)) {
                    muuk::logger::info("'{}' is already installed. Skipping installation.", repo_name);
                    continue;
                }

                muuk::logger::info("Installing '{}': ref={}, git={}", repo_name, ref, git_url);
                clone_shallow_repo(git_url, target_dir, ref);
            }

            muuk::logger::info("Finished installing all dependencies.");
            return {};
        }

        Result<void> remove_package(const std::string& package_name, const std::string& toml_path, const std::string& lockfile_path) {
            muuk::logger::info("Removing package: {}", package_name);

            try {
                if (!fs::exists(toml_path)) {
                    muuk::logger::error("muuk.toml file not found at '{}'", toml_path);
                    return Err("");
                }

                auto result = MuukFiler::create(toml_path);
                if (!result) {
                    muuk::logger::error("Failed to read '{}': {}", toml_path, result.error());
                    return Err("");
                }
                MuukFiler muuk_filer = result.value();

                toml::table& dependencies = muuk_filer.get_section("dependencies");

                if (!dependencies.contains(package_name)) {
                    muuk::logger::error("Package '{}' is not listed in '{}'", package_name, toml_path);
                    return Err("");
                }

                dependencies.erase(package_name);
                muuk_filer.write_to_file();

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
                    delete_hash_file(package_path);
                    fs::remove_all(package_path);
                    muuk::logger::info("Removed package directory: {}", package_path.string());
                } else {
                    muuk::logger::warn("Package directory '{}' does not exist. Skipping.", package_path.string());
                }

                muuk::logger::info("Successfully removed package '{}'", package_name);
            } catch (const std::exception& e) {
                muuk::logger::error("Error removing package '{}': {}", package_name, e.what());
                return Err("");
            }
            return {};
        }
    }
}
