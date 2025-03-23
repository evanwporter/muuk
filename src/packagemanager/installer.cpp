#include <filesystem>
#include <fstream>
#include <string>

#include "logger.h"
#include "muukfiler.h"
#include "muuklockgen.h"
#include "package_manager.h"
#include "rustify.hpp"
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

            muuk::logger::info("Cloning repository: {} into {}", repo_url, target_dir);

            // Backup muuk.toml if it exists
            if (fs::exists(muuk_toml_path)) {
                muuk::logger::info("Backing up existing 'muuk.toml' to '{}'", temp_muuk_toml_path.string());
                fs::rename(muuk_toml_path, temp_muuk_toml_path);
            }

            // Remove existing directory
            if (fs::exists(target_dir)) {
                muuk::logger::info("Removing existing directory '{}'", target_dir);
                fs::remove_all(target_dir);
            }

            auto is_commit_sha = [](const std::string& ref) {
                return ref.size() == 40 && std::all_of(ref.begin(), ref.end(), ::isxdigit);
            };

            bool is_sha = is_commit_sha(checkout_ref);
            std::string clone_cmd = "git clone --single-branch";

            if (!checkout_ref.empty() && !is_sha && checkout_ref != "latest") {
                clone_cmd += " --depth=1 --branch " + checkout_ref;
            }

            clone_cmd += " " + repo_url + " " + target_dir;

            muuk::logger::info("Running clone command: {}", clone_cmd);
            int clone_result = util::command_line::execute_command(clone_cmd);
            if (clone_result != 0) {
                muuk::logger::error("Failed to clone repository '{}'", repo_url);
                return;
            }

            // Attempt checkout if ref is provided and not "latest"
            bool did_checkout = false;
            if (!checkout_ref.empty() && checkout_ref != "latest") {
                std::string checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                muuk::logger::info("Checking out ref: {}", checkout_ref);
                int checkout_result = util::command_line::execute_command(checkout_cmd);

                if (checkout_result == 0) {
                    did_checkout = true;
                } else if (is_sha) {
                    // Retry with full clone
                    muuk::logger::warn("Shallow clone failed to find commit '{}'. Retrying with full clone.", checkout_ref);
                    fs::remove_all(target_dir);

                    std::string full_clone_cmd = "git clone --single-branch " + repo_url + " " + target_dir;
                    if (util::command_line::execute_command(full_clone_cmd) != 0) {
                        muuk::logger::error("Failed to fully clone repository '{}'", repo_url);
                        return;
                    }

                    std::string retry_checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                    if (util::command_line::execute_command(retry_checkout_cmd) != 0) {
                        muuk::logger::error("Still failed to checkout ref '{}'", checkout_ref);
                        return;
                    }

                    did_checkout = true;
                } else {
                    muuk::logger::error("Failed to checkout ref '{}'", checkout_ref);
                    return;
                }
            }

            if (!checkout_ref.empty() && !did_checkout && !is_sha) {
                muuk::logger::warn("No checkout step performed for '{}'.", checkout_ref);
            }

            // Restore muuk.toml if it was backed up
            if (fs::exists(temp_muuk_toml_path)) {
                muuk::logger::info("Restoring 'muuk.toml' from backup");
                fs::rename(temp_muuk_toml_path, muuk_toml_path);
            }

            // Remove .git folder to keep things clean
            fs::path git_dir = target_dir + "/.git";
            if (fs::exists(git_dir)) {
                muuk::logger::info("Removing .git folder from '{}'", target_dir);
                fs::remove_all(git_dir);
            }

            // Mark as installed
            create_hash_file(target_dir);
        }

        Result<void> install(const std::string& lockfile_path_string) {
            const std::string RESET = "\033[0m";
            const std::string BOLD = "\033[1m";
            const std::string GREEN = "\033[32m";
            const std::string YELLOW = "\033[33m";
            const std::string RED = "\033[31m";
            const std::string CYAN = "\033[36m";
            const std::string WHITE = "\033[37m";
            const std::string MAGENTA = "\033[95m";

            fs::path lockfile_path = fs::path(lockfile_path_string);

            std::cout << CYAN << BOLD << "muuk install" << RESET << "\n";

            MuukLockGenerator lockgen("./");
            auto res_lockgen = lockgen.load();
            if (!res_lockgen) {
                return Err("Failed to load muuk.lock.toml: {}", res_lockgen.error());
            }

            auto res_lockfile = lockgen.generate_lockfile(lockfile_path.string());
            if (!res_lockfile) {
                return Err("Failed to generate lockfile: {}", res_lockfile.error());
            }

            std::cout << CYAN << "Reading lockfile: " << lockfile_path << RESET << "\n";

            auto res_muukfiler = MuukFiler::create(lockfile_path.string(), true);
            if (!res_muukfiler) {
                std::cout << RED << "Failed to read lockfile." << RESET << "\n";
                return Err("Failed to read lockfile: {}", res_muukfiler.error());
            }

            MuukFiler muuk_filer = res_muukfiler.value();
            toml::table lockfile_data = muuk_filer.get_config();

            if (!lockfile_data.contains("dependencies") || !lockfile_data["dependencies"].is_table()) {
                std::cout << RED << "No 'dependencies' section found in lockfile." << RESET << "\n";
                return tl::unexpected("No 'dependencies' section found in lockfile");
            }

            toml::table dependencies = *lockfile_data["dependencies"].as_table();

            std::cout << "\nFound " << BOLD << dependencies.size() << RESET << " dependencies:\n";
            for (const auto& [dep_name_, dep_table] : dependencies) {
                std::string repo_name = std::string(dep_name_.str());
                auto dep_info = *dep_table.as_table();
                std::string ref = dep_info.contains("version") ? *dep_info["version"].value<std::string>() : "main";

                std::cout << "  - " << MAGENTA << repo_name << RESET << " @ " << ref.substr(0, 8) << "\n";
            }
            std::cout << "\n";

            for (const auto& [dep_name_, dep_table] : dependencies) {
                std::string repo_name = std::string(dep_name_.str());
                auto dep_info = *dep_table.as_table();
                std::string git_url = dep_info.contains("git") ? *dep_info["git"].value<std::string>() : "";
                std::string ref = dep_info.contains("version") ? *dep_info["version"].value<std::string>() : "main";

                std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name + '/' + ref;

                std::cout << CYAN << "Installing: " << repo_name << " @ " << ref.substr(0, 8) << RESET << "\n";

                if (fs::exists(target_dir) && is_package_installed(target_dir)) {
                    std::cout << YELLOW << "Already installed - skipping.\n"
                              << RESET << "\n";
                    continue;
                }

                std::cout << MAGENTA << "Cloning from " << git_url << RESET << "\n";
                clone_shallow_repo(git_url, target_dir, ref);

                if (fs::exists(target_dir)) {
                    std::cout << GREEN << "Installed " << repo_name << " @ " << ref.substr(0, 8) << RESET << "\n\n";
                } else {
                    std::cout << RED << "Failed to install " << repo_name << RESET << "\n\n";
                }
            }

            std::cout << GREEN << BOLD << "All dependencies are installed!" << RESET << "\n";
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
