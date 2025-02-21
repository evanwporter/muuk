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

                // // Determine which versioning key to use
                // std::string version_key = "version";
                // std::string version_value = version;

                // if (!revision.empty()) {
                //     version_key = "revision";
                //     version_value = revision;
                // }
                // else if (!tag.empty()) {
                //     version_key = "tag";
                //     version_value = tag;
                // }

                // Write default muuk.toml
                muuk_toml << "[package]\n";
                muuk_toml << "name = \"" << repo_name << "\"\n\n";
                // muuk_toml << version_key << " = \"" << version_value << "\"\n\n";

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

        void add_dependency(
            const std::string& toml_path,
            const std::string& repo,
            const std::string& version,
            std::string& git_url,
            std::string& muuk_path,
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
                std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name;

                std::ostringstream toml_string;
                toml_string << dependencies;

                // Ensure dependency folder exists
                util::ensure_directory_exists(DEPENDENCY_FOLDER, true);
                fs::create_directory(target_dir);

                // #1
                if (muuk_path.empty()) {
                    muuk_path = target_dir + "/muuk.toml";

                    // Attempt to fetch the dependency's `muuk.toml`
                    std::string muuk_toml_url = "https://raw.githubusercontent.com/" + author + "/" + repo_name + "/" + revision + "/muuk.toml";

                    bool muuk_toml_downloaded = false;
                    try {
                        logger_->info("Attempting to download muuk.toml from: {}", muuk_toml_url);
                        download_file(muuk_toml_url, muuk_path);
                        muuk_toml_downloaded = fs::exists(muuk_path);
                    }
                    catch (const std::exception& e) {
                        logger_->warn("Could not download muuk.toml from dependency: {}", e.what());
                    }

                    // #2
                    // If not found, attempt to get a patch version
                    if (!muuk_toml_downloaded) {
                        std::string patch_muuk_toml_url = "https://raw.githubusercontent.com/evanwporter/muuk/main/muuk-patches/" + repo_name + "/muuk.toml";
                        try {
                            logger_->info("Attempting to download muuk.toml patch from: {}", patch_muuk_toml_url);
                            download_file(patch_muuk_toml_url, muuk_path);
                            muuk_toml_downloaded = fs::exists(muuk_path);
                        }
                        catch (const std::exception& e) {
                            logger_->warn("Could not download muuk.toml patch: {}", e.what());
                        }
                    }

                    // #3
                    // If neither worked, generate a default `muuk.toml`
                    if (!muuk_toml_downloaded) {
                        logger_->warn("No valid muuk.toml found. Generating a default one.");
                        generate_default_muuk_toml(target_dir, repo_name, version, revision, tag);
                    }
                }

                // Modify the base `muuk.toml` adding to new dependency to the dependencies section
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

                if (has_previous) new_entry << ", ";
                new_entry << "muuk_path = \"" << muuk_path << "\"";
                has_previous = true;

                if (has_previous) new_entry << ", ";
                new_entry << "git = \"" << final_git_url << "\" }";

                // Append new dependency entry to TOML string
                toml_string << "\n" << new_entry.str() << "\n";

                logger_->info("Updated TOML dependencies:\n{}", toml_string.str());

                // Re-parse as a TOML table
                std::istringstream new_table_stream(toml_string.str());
                toml::table new_dependencies = toml::parse(new_table_stream);

                // Replace the `[library.{package.name}.dependencies]` table
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