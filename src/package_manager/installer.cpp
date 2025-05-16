#include <filesystem>
#include <fstream>
#include <string>

#include <toml.hpp>

#include "buildconfig.h"
#include "commands/install.hpp"
#include "lockgen/muuklockgen.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "muukterminal.hpp"
#include "rustify.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {

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

    Result<void> clone_shallow_repo(const std::string& repo_url, const std::string& target_dir, const std::string& checkout_ref) {

        const fs::path muuk_toml_path = target_dir + "/muuk.toml";
        const fs::path parent_repo_dir = fs::path(target_dir).parent_path();
        const fs::path temp_muuk_toml_path = parent_repo_dir / "_muuk_backup.toml";

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

        const bool is_sha = is_commit_sha(checkout_ref);
        std::string clone_cmd = "git clone --single-branch";

        if (!checkout_ref.empty() && !is_sha && checkout_ref != "latest")
            clone_cmd += " --depth=1 --branch " + checkout_ref;

        clone_cmd += " " + repo_url + " " + target_dir;

        muuk::logger::info("Running clone command: {}", clone_cmd);
        const int clone_result = util::command_line::execute_command(clone_cmd);
        if (clone_result != 0)
            return Err("Failed to clone repository '{}'", repo_url);

        // Attempt checkout if ref is provided and not "latest"
        bool did_checkout = false;
        if (!checkout_ref.empty() && checkout_ref != "latest") {
            const std::string checkout_cmd = "cd " + target_dir + " && git -c advice.detachedHead=false checkout " + checkout_ref;
            muuk::logger::info("Checking out ref: {}", checkout_ref);
            const int checkout_result = util::command_line::execute_command(checkout_cmd);

            if (checkout_result == 0) {
                did_checkout = true;
            } else if (is_sha) {
                // Retry with full clone
                muuk::logger::warn("Shallow clone failed to find commit '{}'. Retrying with full clone.", checkout_ref);
                fs::remove_all(target_dir);

                std::string full_clone_cmd = "git clone --single-branch " + repo_url + " " + target_dir;
                if (util::command_line::execute_command(full_clone_cmd) != 0)
                    return Err("Failed to fully clone repository '{}'", repo_url);

                std::string retry_checkout_cmd = "cd " + target_dir + " && git checkout " + checkout_ref;
                if (util::command_line::execute_command(retry_checkout_cmd) != 0)
                    return Err("Still failed to checkout ref '{}'", checkout_ref);

                did_checkout = true;
            } else {
                return Err("Failed to checkout ref '{}'", checkout_ref);
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
        const fs::path git_dir = target_dir + "/.git";
        if (fs::exists(git_dir)) {
            muuk::logger::info("Removing .git folder from '{}'", target_dir);
            fs::remove_all(git_dir);
        }

        // Mark as installed
        create_hash_file(target_dir);
        return {};
    }

    Result<void> install(const std::string& lockfile_path_string) {
        using namespace muuk::terminal;

        const fs::path lockfile_path = fs::path(lockfile_path_string);

        auto lockgen = lockgen::MuukLockGenerator::create("./");
        if (!lockgen) {
            return Err(lockgen.error());
        }

        auto lock_file_result = lockgen->generate_lockfile(lockfile_path.string());
        if (!lock_file_result) {
            return Err("Failed to generate lockfile: {}", lock_file_result.error());
        }

        std::cout << style::CYAN << "Reading lockfile: " << lockfile_path << style::RESET << "\n";

        std::ifstream file(lockfile_path);
        if (!file) {
            return Err("Failed to open lockfile '{}'", lockfile_path_string);
        }

        const auto parse_result = muuk::parse_muuk_file(lockfile_path_string, true);
        if (!parse_result)
            return Err("Failed to parse lockfile '{}': {}", lockfile_path_string, parse_result.error());

        const auto lockfile_data = parse_result.value();

        if (!lockfile_data.contains("package")) {
            return Err("Lockfile does not contain 'package' section");
        }
        const auto packages = lockfile_data.at("package").as_array();

        muuk::terminal::info("Found {}{}{} dependencies:", style::BOLD, packages.size(), style::RESET);
        for (const auto& item : packages) {
            const auto pkg = item.as_table();
            if (!pkg.contains("name") || !pkg.contains("version") || !pkg.contains("source"))
                continue;

            const std::string name = pkg.at("name").as_string();
            const std::string version = pkg.at("version").as_string();
            const std::string source = pkg.at("source").as_string();

            const std::string short_hash = version.substr(0, 8);
            muuk::logger::info("  - {}{}{} @ {}", style::MAGENTA, name, style::RESET, short_hash);
        }

        std::cout << "\n";

        for (const auto& item : packages) {
            const auto pkg = item.as_table();
            if (!pkg.contains("name") || !pkg.contains("version") || !pkg.contains("source"))
                continue;

            const std::string name = pkg.at("name").as_string();
            const std::string version = pkg.at("version").as_string();
            const std::string source = pkg.at("source").as_string();
            const std::string short_hash = version.substr(0, 8);

            muuk::terminal::info("Installing: {}{}{} @ {}{}", style::CYAN, name, style::RESET, short_hash, style::RESET);

            std::string git_url;
            if (source.rfind("git+", 0) == 0) {
                git_url = source.substr(4); // remove "git+"
            } else {
                muuk::logger::warn("Unsupported source format: {}", source);
                continue;
            }

            const std::string target_dir = DEPENDENCY_FOLDER + "/" + name + "/" + version;

            if (fs::exists(target_dir) && is_package_installed(target_dir)) {
                std::cout << style::YELLOW << "Already installed - skipping.\n"
                          << style::RESET << "\n";
                continue;
            }

            std::cout << style::MAGENTA << "Cloning from " << git_url << style::RESET << "\n";
            TRYV(clone_shallow_repo(git_url, target_dir, version));

            if (fs::exists(target_dir)) {
                std::cout << style::GREEN << "Installed " << name << " @ " << short_hash << style::RESET << "\n\n";
            } else {
                std::cout << style::RED << "Failed to install " << name << style::RESET << "\n\n";
            }
        }

        std::cout << style::GREEN << style::BOLD << "All dependencies are installed!" << style::RESET << "\n";
        return {};
    }
}
