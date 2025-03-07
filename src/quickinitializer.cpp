#include "muukterminal.hpp"
#include "muuk.h"
#include "logger.h"
#include "util.h"
#include "buildconfig.h"

#include <fmt/core.h>
#include <tl/expected.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>

namespace fs = std::filesystem;

namespace muuk {

    const std::vector<std::string> common_include_paths = {
       "include", "single_include", "single-include"
    };

    const std::vector<std::string> common_source_paths = {
       "src", "source", "sources"
    };

    Result<std::vector<std::string>> get_top_level_dirs_of_github(const std::string& author, const std::string& repo) {
        std::vector<std::string> top_dirs;
        std::string api_url = "https://api.github.com/repos/" + author + "/" + repo + "/git/trees/master?recursive=1";

        std::string command = "wget -q -O - " + api_url;
        nlohmann::json json_data(util::command_line::execute_command_get_out(command));

        // Extract paths and find top-level directories
        std::unordered_set<std::string> unique_dirs;
        if (json_data.contains("tree")) {
            for (const auto& item : json_data["tree"]) {
                if (item.contains("path") && item.contains("type")) {
                    std::string path = item["path"];
                    std::string type = item["type"];

                    // Only consider directories
                    if (type == "tree") {
                        size_t pos = path.find('/');
                        if (pos != std::string::npos) {
                            unique_dirs.insert(path.substr(0, pos));
                        }
                        else {
                            unique_dirs.insert(path);
                        }
                    }
                }
            }
        }

        if (unique_dirs.empty()) {
            return tl::unexpected("Failed to fetch remote repository structure.");
        }

        // Convert unordered_set to vector
        top_dirs.assign(unique_dirs.begin(), unique_dirs.end());
        return top_dirs;
    }

    Result<std::string> get_license_of_github_repo(const std::string& author, const std::string& repo) {
        std::string api_url = "https://api.github.com/repos/" + author + "/" + repo + "/license";

        auto response = util::network::fetch_json(api_url);
        if (!response) {
            return tl::unexpected("Failed to fetch license JSON for " + author + "/" + repo + ": " + response.error());
        }

        // Ensure `response` is a valid JSON object
        if (!response->is_object()) {
            return tl::unexpected("Invalid JSON response for " + author + "/" + repo);
        }

        // Check if the JSON contains the expected license info
        if (response->contains("license") && (*response)["license"].contains("spdx_id")) {
            return (*response)["license"]["spdx_id"].get<std::string>();
        }

        return tl::unexpected("License information not found for " + author + "/" + repo);
    }


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

    tl::expected<void, std::string> qinit_library(const std::string& author, const std::string& repo) {
        fs::path root = fs::absolute(DEPENDENCY_FOLDER) / repo;

        muuk::logger::info("Initializing library '{}/{}' at '{}'", author, repo, root.string());

        std::vector<fs::path> include_dirs;
        std::vector<fs::path> source_dirs;

        fs::path include_dir;
        fs::path source_dir;

        std::string license = get_license_of_github_repo(author, repo).value_or("");
        std::string revision = util::git::get_latest_revision(fmt::format("https://github.com/{}/{}.git", author, repo));

        muuk::logger::info("Fetching top-level directories from GitHub repo '{}/{}'", author, repo);

        // Get remote directories
        auto remote_dirs = get_top_level_dirs_of_github(author, repo);
        if (!remote_dirs) {
            logger::warn(remote_dirs.error());

            source_dir = "";
            include_dir = "";
        }
        else {
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
            source_dir = include_dir_result.value_or("src");
        }

        std::ofstream config_file(root / "muuk.toml", std::ios::out | std::ios::trunc);
        if (!config_file.is_open()) {
            return tl::unexpected(fmt::format("Failed to create muuk.toml in {}", root.string()));
        }

        config_file << "[package]\n"
            << "name = '" << repo << "'\n"
            << "author = '" << author << "'\n"
            << "license = '" << license << "'\n\n";

        config_file << "[library]\n"
            << "include = ['" << include_dir.string() << "']\n"
            << "sources = '" << source_dir.string() << "'\n";
        config_file.close();

        muuk::logger::info("Successfully initialized '{}' with muuk.toml", repo);
        return {};
    }

} // namespace muuk 
