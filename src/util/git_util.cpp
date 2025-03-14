#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "logger.h"
#include "rustify.hpp"
#include "util.h"

namespace util {
    // ==========================
    //  Git Utilities
    // ==========================
    namespace git {

        Result<std::string> get_default_branch(const std::string& git_url) {
            std::string branch_cmd = "git ls-remote --symref " + git_url + " HEAD";
            std::string output = command_line::execute_command_get_out(branch_cmd);

            size_t start = output.find("refs/heads/");
            if (start == std::string::npos) {
                muuk::logger::error("Failed to retrieve default branch for '{}'.", git_url);
                Err("");
            }

            start += 11; // Move "cursor" past "refs/heads/"
            size_t end = output.find('\n', start);
            std::string default_branch = output.substr(start, end - start);

            muuk::logger::info("Default branch for {} is {}", git_url, default_branch);
            return default_branch;
        }

        Result<std::string> get_default_branch(const std::string& author, const std::string& repo) {
            return get_default_branch(fmt::format("https://github.com/{}/{}.git", author, repo));
        }

        // Function to fetch repository tree JSON
        // Function to fetch repository tree JSON
        Result<nlohmann::json> fetch_repo_tree(const std::string& author, const std::string& repo, const std::string& branch) {
            std::string api_url = "https://api.github.com/repos/" + author + "/" + repo + "/git/trees/" + branch + "?recursive=1";
            std::string command = "wget -q -O - " + api_url;

            try {
                nlohmann::json json_data = nlohmann::json(util::command_line::execute_command_get_out(command));
                muuk::logger::trace("Fetched repository tree JSON: {}", json_data.dump());

                if (json_data.is_string()) {
                    muuk::logger::info("JSON appears to be a string. Parsing again...");
                    json_data = nlohmann::json::parse(json_data.get<std::string>());
                }

                muuk::logger::info("JSON parsed successfully. Type: {}", json_data.type_name());

                if (!json_data.is_object()) {
                    muuk::logger::error("JSON root is not an object!");
                    return tl::unexpected("Unexpected JSON structure.");
                }

                if (!json_data.contains("tree")) {
                    muuk::logger::error("JSON does not contain expected 'tree' key.");
                    return tl::unexpected("Unexpected JSON format.");
                }
                return json_data;
            } catch (const std::exception& e) {
                muuk::logger::error("Error fetching repository tree: {}", e.what());
                return tl::unexpected("Failed to fetch repository structure.");
            }
        }

        // Function to extract top-level directories from repository JSON
        std::vector<std::string> extract_top_level_dirs(const nlohmann::json& json_data) {
            std::unordered_set<std::string> unique_dirs;

            for (const auto& item : json_data["tree"]) {
                if (item.contains("path") && item.contains("type") && item["type"] == "tree") {
                    std::string path = item["path"];
                    size_t pos = path.find('/');

                    if (pos != std::string::npos) {
                        unique_dirs.insert(path.substr(0, pos));
                    } else {
                        unique_dirs.insert(path);
                    }
                }
            }

            return std::vector<std::string>(unique_dirs.begin(), unique_dirs.end());
        }

        Result<std::vector<std::string>> get_top_level_dirs_of_github(const std::string& author, const std::string& repo) {
            auto result = get_default_branch(author, repo);
            if (!result) {
                return tl::unexpected(result.error());
            }
            std::string branch = result.value();

            auto json_result = fetch_repo_tree(author, repo, branch);
            if (!json_result) {
                return tl::unexpected(json_result.error());
            }

            std::vector<std::string> top_dirs = extract_top_level_dirs(json_result.value());
            if (top_dirs.empty()) {
                return tl::unexpected("Failed to fetch remote repository structure.");
            }

            return top_dirs;
        }

        std::string get_latest_revision(const std::string& git_url) {
            std::string commit_hash_cmd = "git ls-remote " + git_url + " HEAD";
            std::string output = command_line::execute_command_get_out(commit_hash_cmd);
            std::string revision = output.substr(0, output.find('\t'));

            if (revision.empty()) {
                muuk::logger::error("Failed to retrieve latest commit hash for '{}'.", git_url);
                throw std::runtime_error("Failed to retrieve latest commit hash.");
            }

            revision.erase(std::remove(revision.begin(), revision.end(), '\n'), revision.end());
            muuk::logger::info("Latest commit hash for {} is {}", git_url, revision);

            return revision;
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

    } // namespace git
} // namespace util