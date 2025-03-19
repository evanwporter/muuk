#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include "buildconfig.h"
#include "logger.h"
#include "muuk.h"
#include "muukterminal.hpp"
#include "util.h"

namespace fs = std::filesystem;

namespace muuk {

    const std::vector<std::string> common_include_paths = {
        "include", "single_include", "single-include"
    };

    const std::vector<std::string> common_source_paths = {
        "src", "source", "sources"
    };

    tl::expected<fs::path, std::string> select_directory(const std::vector<fs::path>& directories, const std::string& type) {
        if (directories.empty()) {
            return tl::unexpected("No directories found for " + type);
        }
        if (directories.size() == 1) {
            return directories[0];
        }

        std::vector<std::string> dir_names;
        for (const auto& dir : directories) {
            dir_names.push_back(dir.string());
        }

        std::cout << "Select a " << type << " directory" << std::endl;
        int selected_index = muuk::terminal::select_from_menu(dir_names);
        return directories[selected_index];
    }

    std::vector<fs::path> detect_directories(const fs::path& root, const std::vector<std::string>& candidates) {
        std::vector<fs::path> found;
        for (const auto& dir : candidates) {
            fs::path candidate = root / dir;
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                found.push_back(candidate);
            }
        }
        return found;
    }

    tl::expected<void, std::string> qinit_library(const std::string& author, const std::string& repo, const std::string& version) {
        fs::path root = fs::absolute(DEPENDENCY_FOLDER) / repo;

        muuk::logger::info("Initializing library '{}/{}' at '{}'", author, repo, root.string());

        std::vector<fs::path> include_dirs;
        std::vector<fs::path> source_dirs;

        fs::path include_dir;
        fs::path source_dir;

        muuk::logger::info("Fetching license from GitHub for '{}/{}'...", author, repo);
        std::string license = util::git::get_license_of_github_repo(author, repo).value_or("Unknown");
        muuk::logger::info("Detected license: {}", license);

        muuk::logger::info("Fetching top-level directories from GitHub repo '{}/{}'", author, repo);

        // Get remote directories
        auto remote_dirs = util::git::get_top_level_dirs_of_github(author, repo);
        if (!remote_dirs) {
            logger::warn("Failed to fetch remote directories: {}", remote_dirs.error());
            source_dir = "";
            include_dir = "";
        } else {
            for (const auto& dir : remote_dirs.value()) {
                logger::info("Found remote directory: {}", dir);
            }

            // Ask user to select include and source directories
            std::vector<fs::path> remote_include_dirs, remote_source_dirs;
            for (const auto& dir : remote_dirs.value()) {
                fs::path p = root / dir;
                if (std::find(common_include_paths.begin(), common_include_paths.end(), dir) != common_include_paths.end()) {
                    remote_include_dirs.push_back(dir);
                }
                if (std::find(common_source_paths.begin(), common_source_paths.end(), dir) != common_source_paths.end()) {
                    remote_source_dirs.push_back(dir);
                }
            }

            include_dirs = remote_include_dirs;
            source_dirs = remote_source_dirs;

            // Allow user selection if multiple directories are found
            auto include_dir_result = select_directory(include_dirs, "include");
            auto source_dir_result = select_directory(source_dirs, "source");

            include_dir = include_dir_result.value_or("include");
            source_dir = source_dir_result.value_or("src");
        }

        util::ensure_directory_exists((root / version).string());

        // Construct the TOML file content
        std::stringstream toml_content;
        toml_content << "[package]\n"
                     << "name = '" << repo << "'\n"
                     << "author = '" << author << "'\n"
                     << "license = '" << license << "'\n"
                     << "version = '" << version << "'\n"
                     << "git = 'https://github.com/" << author << "/" << repo << ".git'\n\n";

        toml_content << "[library]\n"
                     << "include = ['" << include_dir.string() << "']\n"
                     << "sources = '" << source_dir.string() << "'\n";

        // Print the TOML content to stdout
        std::cout << "Generated muuk.toml content:\n";
        std::cout << toml_content.str() << std::endl;

        // Save the TOML content to a file
        fs::path toml_path = root / version / "muuk.toml";
        std::ofstream config_file(toml_path, std::ios::out | std::ios::trunc);
        if (!config_file.is_open()) {
            muuk::logger::error("Failed to create muuk.toml in {}", root.string());
            return Err("");
        }

        muuk::logger::info("Writing muuk.toml to '{}'", toml_path.string());
        config_file << toml_content.str();
        config_file.close();

        muuk::logger::info("Successfully initialized '{}' with muuk.toml", repo);
        return {};
    }

} // namespace muuk
