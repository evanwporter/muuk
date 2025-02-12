#include "../include/muuk.h"
#include "../include/muukfiler.h"
#include "../include/util.h"
#include "../include/logger.h"
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <glob.hpp>

namespace fs = std::filesystem;

Muuk::Muuk(IMuukFiler& config_manager) : config_manager_(config_manager), muuk_builder_(config_manager) {
    logger_ = Logger::get_logger("muuk_logger");
}

void Muuk::clean() const {
    logger_->info("[muuk::clean] Starting clean operation.");

    if (!config_manager_.has_section("clean")) {
        logger_->warn("[muuk::clean] 'clean' operation is not defined in the config file.");
        return;
    }

    fs::path current_dir = fs::current_path();
    logger_->info("[muuk::clean] Current working directory: {}", current_dir.string());

    if (!fs::exists(current_dir)) {
        logger_->warn("[muuk::clean] Skipping clean operation: Directory '{}' does not exist.", current_dir.string());
        return;
    }

    auto clean_section = config_manager_.get_section("clean");
    auto clean_patterns = clean_section.get_as<toml::array>("patterns");

    if (!clean_patterns) {
        logger_->warn("[muuk::clean] 'clean.patterns' is not a valid array.");
        return;
    }

    // Log detected cleaning patterns
    logger_->info("[muuk::clean] Cleaning patterns:");
    for (const auto& pattern : *clean_patterns) {
        if (pattern.is_string()) {
            logger_->info(" - {}", *pattern.value<std::string>());
        }
    }

    std::vector<fs::path> files_to_delete;

    try {
        // Collect all matching files first
        for (const auto& pattern : *clean_patterns) {
            if (pattern.is_string()) {
                std::string pattern_str = *pattern.value<std::string>();
                std::vector<std::string> matched_files = glob::glob(pattern_str, current_dir.string());

                for (const auto& file_path : matched_files) {
                    files_to_delete.push_back(fs::path(file_path));
                }
            }
        }


        // Delete collected files
        for (const auto& file : files_to_delete) {
            try {
                logger_->info("[muuk::clean] Removing file: {}", file.string());
                util::remove_path(file.string());
            }
            catch (const std::exception& e) {
                logger_->warn("[muuk::clean] Failed to remove '{}': {}", file.string(), e.what());
            }
        }
    }
    catch (const std::exception& e) {
        logger_->error("[muuk::clean] Error during directory iteration: {}", e.what());
    }

    logger_->info("[muuk::clean] Clean operation completed.");
}

void Muuk::run_script(const std::string& script, const std::vector<std::string>& args) const {
    logger_->info("[muuk::run] Running script: {}", script);

    const auto& config = config_manager_.get_config();
    auto scripts_section = config.get_as<toml::table>("scripts");
    if (!scripts_section || !scripts_section->contains(script)) {
        logger_->error("[muuk::run] Script '{}' not found in the config file.", script);
        return;
    }

    auto script_entry = scripts_section->get(script);
    if (!script_entry || !script_entry->is_string()) {
        logger_->error("[muuk::run] Script '{}' must be a string command in the config file.", script);
        return;
    }

    std::string command = *script_entry->value<std::string>();
    for (const auto& arg : args) {
        command += " " + arg;
    }

    logger_->info("[muuk::run] Executing command: {}", command);
    int result = util::execute_command(command.c_str());
    if (result != 0) {
        logger_->error("[muuk::run] Command failed with error code: {}", result);
    }
    else {
        logger_->info("[muuk::run] Command executed successfully.");
    }
}


void Muuk::build(const std::vector<std::string>& args) const {
    logger_->info("Starting build operation.");
    // muuk_builder_.build(args);
}

void Muuk::download_github_release(const std::string& repo, const std::string& version) {
    logger_->info("[muuk::install] Adding GitHub repository as a submodule. Repository: {}, Version: {}", repo, version);

    size_t slash_pos = repo.find('/');
    if (slash_pos == std::string::npos) {
        logger_->error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
        return;
    }

    std::string author = repo.substr(0, slash_pos);
    std::string repo_name = repo.substr(slash_pos + 1);
    std::string resolved_version = version;

    // If version is "latest", fetch the latest commit or tag
    if (version == "latest") {
        std::string api_url = "https://api.github.com/repos/" + author + "/" + repo_name + "/releases/latest";
        logger_->info("[muuk::install] Fetching latest release from: {}", api_url);

        try {
            nlohmann::json response = util::fetch_json(api_url);
            if (response.contains("tag_name") && response["tag_name"].is_string()) {
                resolved_version = response["tag_name"].get<std::string>();
                logger_->info("[muuk::install] Resolved latest version: {} of {}/{}", resolved_version, author, repo_name);
            }
            else {
                logger_->warn("[muuk::install] No release found, defaulting to latest commit.");
                resolved_version = "latest";  // Default to latest commit if no release is found
            }
        }
        catch (const std::exception& e) {
            logger_->error("[muuk::install] Error fetching latest release: {}", e.what());
            return;
        }
    }

    // Ensure the "modules" directory exists
    const std::string modules_folder = "modules";
    util::ensure_directory_exists(modules_folder, true);

    std::string submodule_path = modules_folder + "/" + repo_name;

    // Check if submodule already exists
    if (fs::exists(submodule_path)) {
        logger_->info("[muuk::install] Submodule '{}' already exists. Running update instead.", repo_name);
        util::execute_command("git submodule update --remote --recursive " + submodule_path);
    }
    else {
        // Add the submodule
        std::string git_url = "https://github.com/" + author + "/" + repo_name + ".git";
        std::string add_command = "git submodule add " + git_url + " " + submodule_path;
        logger_->info("[muuk::install] Running command: {}", add_command);

        int result = util::execute_command(add_command);
        if (result != 0) {
            logger_->error("[muuk::install] Failed to add submodule '{}'", repo_name);
            return;
        }

        // Initialize and update submodule
        util::execute_command("git submodule update --init --recursive " + submodule_path);
    }

    // Checkout specific version if needed
    if (resolved_version != "latest") {
        std::string checkout_command = "cd " + submodule_path + " && git checkout " + resolved_version;
        logger_->info("[muuk::install] Checking out version '{}': {}", resolved_version, checkout_command);

        int checkout_result = util::execute_command(checkout_command);
        if (checkout_result != 0) {
            logger_->warn("[muuk::install] Failed to checkout version '{}'. Sticking to the latest commit.", resolved_version);
        }
    }

    add_dependency(author, repo_name, resolved_version);
}

void Muuk::add_dependency(const std::string& author, const std::string& repo_name, const std::string& version) {
    logger_->info("[muuk::install] Adding/updating dependency: {}-{} - Version: {}", author, repo_name, version);

    if (!config_manager_.has_section("dependencies")) {
        logger_->info("[muuk::install] Creating 'dependencies' section in config.");
        config_manager_.update_section("dependencies", toml::table{});
    }

    toml::table dependencies = config_manager_.get_section("dependencies");
    std::string key = author + "-" + repo_name;

    auto existing_version = dependencies.get(key);
    if (existing_version && existing_version->is_string()) {
        if (*existing_version->value<std::string>() == version) {
            logger_->info("[muuk::install] Dependency {} is already at version {}.", key, version);
        }
        else {
            dependencies.insert_or_assign(key, version);
            logger_->info("[muuk::install] Updated dependency {} to version {}.", key, version);
        }
    }
    else {
        dependencies.insert_or_assign(key, version);
        logger_->info("[muuk::install] Added new dependency: {} - Version: {}", key, version);
    }

    config_manager_.update_section("dependencies", dependencies);
    logger_->info("[muuk::install] Dependencies updated successfully.");
}