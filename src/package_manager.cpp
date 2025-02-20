#include "logger.h"
#include "buildconfig.h"
#include "util.h"
#include "package_manager.h"
#include "muukfiler.h"

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

        std::shared_ptr<spdlog::logger> logger_ = logger::get_logger("muuk::installer");

        void extract_zip(const std::string& archive, const std::string& target_dir) {
            try {
                util::ensure_directory_exists(target_dir);

                if (!fs::exists(archive)) {
                    logger_->error("Zip file does not exist: {}", archive);
                    throw std::runtime_error("Zip file not found: " + archive);
                }

                logger_->info("Starting extraction of zip archive: {} from {}", archive, target_dir);

                int result = zip_extract(
                    archive.c_str(),
                    target_dir.c_str(),
                    [](const char* filename, void* arg) -> int {
                        // logger_->info("Extracting: {}", filename);
                        (void)filename;
                        (void)arg;
                        return 0;
                    },
                    nullptr
                );

                if (result < 0) {
                    logger_->error("Failed to extract zip archive '{}'. Error code: {}", archive, result);
                    throw std::runtime_error("Zip extraction failed.");
                }

                logger_->info("Extraction completed successfully for: {}", archive);
            }
            catch (const std::exception& ex) {
                logger_->error("Exception during zip extraction: {}", ex.what());
                throw;
            }
            catch (...) {
                logger_->error("Unknown error occurred during zip extraction.");
                throw;
            }
        }

        void download_file(const std::string& url, const std::string& output_path) {
            std::string command;

            if (util::command_exists("wget")) {
                command = "wget --quiet --output-document=" + output_path +
                    " --no-check-certificate " + url;
            }
            else if (util::command_exists("curl")) {
                command = "curl -L -o " + output_path + " " + url;
            }
            else {
                logger_->error("Neither wget nor curl is available on the system.");
                throw std::runtime_error("No suitable downloader found. Install wget or curl.");
            }

            logger_->info("Executing download command: {}", command);

            int result = util::execute_command(command.c_str());
            if (result != 0) {
                logger_->error("Failed to download file from {}. Command exited with code: {}", url, result);
                throw std::runtime_error("File download failed.");
            }
        }

        std::pair<std::string, std::string> split_author_repo(std::string repo) {
            size_t slash_pos = repo.find('/');
            if (slash_pos == std::string::npos) {
                logger_->error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
                return { "", "" };
            }

            std::string author = repo.substr(0, slash_pos);
            std::string repo_name = repo.substr(slash_pos + 1);

            if (repo_name.empty()) {
                logger_->error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
                return { "", "" };
            }

            return { author, repo_name };
        }

        void download_github_release(const std::string& repo, const std::string& version) {
            logger_->info("Downloading GitHub release. Repository: {}, Version: {}", repo, version);

            auto [author, repo_name] = split_author_repo(repo);
            if (repo_name.empty()) {
                return;
            }

            std::string resolved_version = version;

            // If version is "latest", fetch the latest release version
            if (version == "latest") {
                std::string api_url = "https://api.github.com/repos/" + author + "/" + repo_name + "/releases/latest";
                logger_->info("Fetching latest release from: {}", api_url);

                try {
                    nlohmann::json response = util::fetch_json(api_url);
                    if (response.contains("tag_name") && response["tag_name"].is_string()) {
                        resolved_version = response["tag_name"].get<std::string>();
                        logger_->info("Resolved latest version: {} of {}/{}", resolved_version, author, repo_name);
                    }
                    else {
                        logger_->error("Failed to fetch latest release version of {}/{}", author, repo_name);
                        return;
                    }
                }
                catch (const std::exception& e) {
                    logger_->error("Error fetching latest release: {}", e.what());
                    return;
                }
            }

            // Construct the GitHub release download URL
            std::string archive_url = "https://github.com/" + author + "/" + repo_name + "/archive/refs/tags/" + resolved_version + ".zip";

            util::ensure_directory_exists(DEPENDENCY_FOLDER, true);

            const auto deps_path = std::string(DEPENDENCY_FOLDER);
            std::string zip_path = deps_path + "/tmp.zip";
            std::string expected_extracted_folder = deps_path + "/" + repo_name + "-" + resolved_version;
            std::string renamed_folder = deps_path + "/" + author + "-" + repo_name + "-" + resolved_version;

            try {
                logger_->info("Downloading file from URL: {}", archive_url);
                download_file(archive_url, zip_path);

                logger_->info("Extracting downloaded file: {}", zip_path);
                extract_zip(zip_path, deps_path);

                // Detect the extracted folder
                bool renamed = false;
                for (const auto& entry : fs::directory_iterator(deps_path)) {
                    if (entry.is_directory() && entry.path().filename().string().find(repo_name) == 0) {
                        fs::rename(entry.path(), renamed_folder);
                        logger_->info("Renamed extracted folder '{}' to '{}'", entry.path().string(), renamed_folder);
                        renamed = true;
                        break;
                    }
                }

                if (!renamed) {
                    logger_->error("Could not find the expected extracted folder: {}", expected_extracted_folder);
                }

                // Clean up ZIP file
                util::remove_path(zip_path);

                // download_patch(author, repo_name, resolved_version);
                // add_dependency(author, repo_name, resolved_version);
            }
            catch (const std::exception& e) {
                logger_->error("Error downloading GitHub repository: {}", e.what());
            }
        }

        void install_submodule(const std::string& repo) {
            logger_->info("Installing Git submodule: {}", repo);

            size_t slash_pos = repo.find('/');
            if (slash_pos == std::string::npos) {
                logger_->error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
                return;
            }

            std::string author = repo.substr(0, slash_pos);
            std::string repo_name = repo.substr(slash_pos + 1);
            std::string submodule_path = "deps/" + repo_name;

            util::ensure_directory_exists("deps");

            std::string git_command = "git submodule add https://github.com/" + repo + ".git " + submodule_path;
            logger_->info("Executing: {}", git_command);

            int result = util::execute_command(git_command.c_str());
            if (result != 0) {
                logger_->error("Failed to add submodule '{}'.", repo);
                return;
            }

            std::string init_command = "git submodule update --init --recursive " + submodule_path;
            logger_->info("Initializing submodule...");
            result = util::execute_command(init_command.c_str());

            if (result != 0) {
                logger_->error("Failed to initialize submodule '{}'.", repo);
                return;
            }

            // Retrieve the latest tag/version of the submodule
            std::string get_version_cmd = "cd " + submodule_path + " && git describe --tags --abbrev=0";
            std::string version = "";// = util::execute_command(get_version_cmd);

            if (version.empty()) {
                version = "latest"; // Fallback in case no tags are found
            }

            logger_->info("Submodule '{}' installed with version '{}'.", repo_name, version);

        }

        void add_dependency(
            const std::string& toml_path,
            const std::string& repo,
            const std::string& version,
            std::string& git_url,
            const std::string& muuk_path,
            std::string revision,
            const std::string& tag,
            const std::string& branch,
            bool is_system,
            const std::string& target_section
        ) {

            logger_->info("Adding dependency to '{}': {} (version: {}, revision: {}, branch: {})",
                toml_path, repo, version, revision, branch);

            try {
                if (!fs::exists(toml_path)) {
                    logger_->error("muuk.toml file not found at: {}", toml_path);
                    throw std::runtime_error("muuk.toml file not found.");
                }

                MuukFiler muukFiler(toml_path);

                // Step 1: Get package name
                toml::table package_section = muukFiler.get_section("package");
                if (!package_section.contains("name") || !package_section["name"].is_string()) {
                    logger_->error("Missing 'name' in [package] section.");
                    throw std::runtime_error("Invalid muuk.toml: missing package.name");
                }
                std::string package_name = *package_section["name"].value<std::string>();

                std::string dependencies_section;
                if (target_section.empty()) {
                    dependencies_section = "library." + package_name + ".dependencies";
                }
                else {
                    dependencies_section = target_section + ".dependencies";
                }

                toml::table& dependencies = muukFiler.get_section(dependencies_section);

                // Step 3: Extract only the repo name using split_author_repo
                auto [author, repo_name] = split_author_repo(repo);
                if (repo_name.empty()) {
                    throw std::runtime_error("Invalid repository format. Expected <author>/<repo>");
                }

                if (dependencies.contains(repo_name)) {
                    logger_->info("Dependency '{}' already exists in '{}'. Skipping.", repo_name, toml_path);
                    throw std::runtime_error(std::format("Dependency '{}' already exists in '{}'.", repo_name, toml_path));
                }

                if (!tag.empty()) {
                    revision = tag;
                    logger_->info("Using tag: {}", tag);
                }

                else if (!revision.empty()) {
                    logger_->info("Using specific commit hash: {}", revision);
                }

                else if (!is_system) {
                    if (git_url.empty()) {
                        git_url = "https://github.com/" + repo + ".git";
                    }
                    logger_->info("No tag, version, or revision provided. Fetching latest commit hash...");

                    std::string commit_hash_cmd = "git ls-remote " + git_url + " HEAD";
                    std::string output = util::execute_command_get_out(commit_hash_cmd);
                    revision = output.substr(0, output.find('\t'));

                    logger_->info("Latest commit hash for {} is {}", git_url, revision);

                    revision.erase(std::remove(revision.begin(), revision.end(), '\n'), revision.end());

                    if (revision.empty()) {
                        logger_->error("Failed to retrieve latest commit hash for '{}'", repo_name);
                        throw std::runtime_error("Failed to get latest commit hash.");
                    }

                    logger_->info("Fetched latest commit hash: {}", revision);
                }


                std::string final_git_url = git_url.empty() ? "https://github.com/" + author + "/" + repo_name + ".git" : git_url;

                // Step 5: Convert existing dependencies section to a string
                std::ostringstream toml_string;
                toml_string << dependencies;

                // Step 6: Manually construct the TOML entry
                std::ostringstream new_entry;
                new_entry << repo_name << " = { ";
                bool has_previous = false;

                if (!version.empty()) {
                    new_entry << "version = \"" << version << "\"";
                    has_previous = true;
                }
                if (!revision.empty()) {
                    if (has_previous) new_entry << ", ";
                    new_entry << "revision = \"" << revision << "\"";
                    has_previous = true;
                }
                if (!branch.empty()) {
                    if (has_previous) new_entry << ", ";
                    new_entry << "branch = \"" << branch << "\"";
                    has_previous = true;
                }
                if (!muuk_path.empty()) {
                    if (has_previous) new_entry << ", ";
                    new_entry << "muuk_path = \"" << muuk_path << "\"";
                    has_previous = true;
                }
                if (has_previous) new_entry << ", ";
                new_entry << "git = \"" << final_git_url << "\" }";

                // Step 7: Append new dependency entry to TOML string
                toml_string << "\n" << new_entry.str() << "\n";

                logger_->info("Updated TOML dependencies:\n{}", toml_string.str());

                // Step 8: Re-parse as a TOML table
                std::istringstream new_table_stream(toml_string.str());
                toml::table new_dependencies = toml::parse(new_table_stream);

                // Step 9: Replace the `[library.{package.name}.dependencies]` table
                muukFiler.modify_section(dependencies_section, new_dependencies);
                muukFiler.write_to_file();

                logger_->info("Successfully added dependency '{}' to '{}'", repo_name, toml_path);
            }
            catch (const std::exception& e) {
                logger_->error("Error adding dependency to muuk.toml: {}", e.what());
                throw;
            }
        }
    }
}