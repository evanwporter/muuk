#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
#include <toml.hpp>

extern "C" {
#include "zip.h"
}
#include "buildconfig.h"
#include "commands/add.hpp"
#include "logger.h"
#include "muuk.h"
#include "rustify.hpp"
#include "util.h"

constexpr const char* MUUK_PATCH_UTL = "https://raw.githubusercontent.com/evanwporter/muuk/main/muuk-patches/";

namespace fs = std::filesystem;

namespace muuk {
    void extract_zip(const std::string& archive, const std::string& target_dir) {
        try {
            util::ensure_directory_exists(target_dir);

            if (!fs::exists(archive)) {
                muuk::logger::error("Zip file does not exist: {}", archive);
                throw std::runtime_error("Zip file not found: " + archive);
            }

            muuk::logger::info("Starting extraction of zip archive: {} from {}", archive, target_dir);

            int result = zip_extract(
                archive.c_str(),
                target_dir.c_str(),
                [](const char* filename, void* arg) -> int {
                    muuk::logger::trace("  → Extracting: {}", filename);
                    (void)filename;
                    (void)arg;
                    return 0;
                },
                nullptr);

            if (result < 0) {
                muuk::logger::error("Failed to extract zip archive '{}'. Error code: {}", archive, result);
                throw std::runtime_error("Zip extraction failed.");
            }

            muuk::logger::info("Extraction completed successfully for: {}", archive);
        } catch (const std::exception& ex) {
            muuk::logger::error("Exception during zip extraction: {}", ex.what());
            throw;
        } catch (...) {
            muuk::logger::error("Unknown error occurred during zip extraction.");
            throw;
        }
    }

    Result<void> download_file(const std::string& url, const std::string& output_path) {
        std::string command;

        if (util::command_line::command_exists("wget")) {
            command = "wget --quiet --output-document=" + output_path + " --no-check-certificate " + url;
        } else if (util::command_line::command_exists("curl")) {
            command = "curl -L -o " + output_path + " " + url;
        } else {
            muuk::logger::error("Neither wget nor curl is available on the system.");
            throw std::runtime_error("No suitable downloader found. Install wget or curl.");
        }

        muuk::logger::info("Executing download command: {}", command);

        int result = util::command_line::execute_command(command.c_str());
        if (result != 0) {
            return tl::unexpected("File download failed with exit code: " + std::to_string(result));
        }

        return {};
    }

    std::pair<std::string, std::string> split_author_repo(std::string repo) {
        size_t slash_pos = repo.find('/');
        if (slash_pos == std::string::npos) {
            muuk::logger::error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
            return { "", "" };
        }

        std::string author = repo.substr(0, slash_pos);
        std::string repo_name = repo.substr(slash_pos + 1);

        if (repo_name.empty()) {
            muuk::logger::error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
            return { "", "" };
        }

        return { author, repo_name };
    }

    Result<void> add(const std::string& toml_path, const std::string& repo, std::string version, std::string& git_url, std::string& muuk_path, bool is_system, const std::string& target_section) {
        muuk::logger::info(
            "Adding dependency to '{}': {} (version: {})",
            toml_path,
            repo,
            version);

        try {
            if (!fs::exists(toml_path)) {
                return Err("muuk.toml file not found: {}", toml_path);
            }

            auto root = toml::parse<toml::ordered_type_config>(toml_path);

            // Get the package name
            if (!root.contains("package") || !root.at("package").is_table())
                return Err("Missing or invalid 'package' section.");
            const auto& package_table = root.at("package").as_table();

            if (!package_table.contains("name") || !package_table.at("name").is_string())
                return Err("Missing 'name' field in 'package' section.");
            const std::string package_name = package_table.at("name").as_string();

            if (!root.contains("dependencies") || !root.at("dependencies").is_table())
                return Err("Missing or invalid 'dependencies' section.");
            auto& dependencies = root.at("dependencies").as_table();

            auto [author, repo_name] = split_author_repo(repo);
            if (repo_name.empty())
                return Err(
                    "Invalid repository format. Expected <author>/<repo>");

            if (dependencies.contains(repo_name))
                return Err(
                    "Dependency '{}' already exists in '{}'.",
                    repo_name,
                    toml_path);

            // Determine Git URL and version
            if (!is_system) {
                if (git_url.empty())
                    git_url = "https://github.com/" + repo + ".git";

                muuk::logger::info("No tag, version, or revision provided. Fetching latest commit hash...");
                if (version.empty())
                    version = util::git::get_latest_revision(git_url);
            }

            std::string final_git_url = git_url.empty()
                ? "https://github.com/" + author + "/" + repo_name + ".git"
                : git_url;
            std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name + "/" + version;

            // Ensure dependency folder exists
            util::ensure_directory_exists(DEPENDENCY_FOLDER, true);
            util::ensure_directory_exists(target_dir);

            if (muuk_path.empty()) {
                muuk_path = target_dir + "/muuk.toml";
                std::string muuk_toml_url = "https://raw.githubusercontent.com/" + author + "/" + repo_name + "/" + version + "/muuk.toml";

                if (!download_file(muuk_toml_url, muuk_path)) {
                    muuk::logger::warn("Failed to download muuk.toml from repo, checking patch...");
                    std::string patch_muuk_toml_url = MUUK_PATCH_UTL + repo_name + "/muuk.toml";
                    if (!download_file(patch_muuk_toml_url, muuk_path)) {
                        muuk::logger::warn("No valid patch found. Generating a default `muuk.toml`.");
                        if (!qinit_library(author, repo_name, version))
                            return Err("Failed to generate default muuk.toml.");
                    }
                }
            }

            dependencies.emplace_back(repo_name, toml::value(version));

            std::ofstream file_out(toml_path, std::ios::trunc);
            if (!file_out.is_open()) {
                return tl::unexpected("Failed to open TOML file for writing: " + toml_path);
            }

            file_out << toml::format(root);
            file_out.close();

            muuk::logger::info("Added dependency '{}' to '{}'", repo_name, toml_path);
            return {};
        } catch (const std::exception& e) {
            return tl::unexpected("Error adding dependency: " + std::string(e.what()));
        }
    }
}
