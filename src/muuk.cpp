//                               @//////////////&/                                 
//                           &///////////////////////                              
//                        %%  , &////////##*@, (   (####                           
//                       %#/#(////////(##///(#(//#(#/////#@.                       
//                      */////#########////##////%#&&//////%///&#                  
//                     /####/////////////##//&&@#@&&&////##/@####////%             
//                 *//////&&&@#////////#//#****#/@&&&&//////#(//##///##&           
//                @////##%%&&&&&@(######&********(**@&///////@/#####///%(          
//               @##///###&&&********************((***&//////###////##%@&&         
//           .(/////###/@&&**********/(((/******/(#**&/////##//////////#%/         
//         */////////////////////////%&&(////////////////@####/######//(/////@     
//     #%@##/####///#&///////////////#####////&&(///////#///////////####&#######   
// #/##////////////////*@####@##////###(%##///#####&//////##/////////#####///&(//# 
//       #///####(((########                       ######&@%@##(//###     @##(&   
// ?.:+*++ = : .        .. : ---- : .      .   ...     .. : = ++ += .. .       .

#include "../include/muuk.h"
#include "../include/muukfiler.h"
#include "../include/util.h"
#include "../include/logger.h"
#include "../include/buildconfig.h"

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <glob/glob.hpp>
#include <unordered_set>
#include "muuk.h"

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
                std::string full_pattern = (fs::path(current_dir) / pattern_str).string();
                std::vector<std::filesystem::path> matched_files = glob::glob(full_pattern);

                for (const auto& file_path : matched_files) {
                    files_to_delete.push_back(file_path);
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

void Muuk::download_github_release(const std::string& repo, const std::string& version) {
    logger_->info("[muuk::install] Downloading GitHub release. Repository: {}, Version: {}", repo, version);

    size_t slash_pos = repo.find('/');
    if (slash_pos == std::string::npos) {
        logger_->error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
        return;
    }

    std::string author = repo.substr(0, slash_pos);
    std::string repo_name = repo.substr(slash_pos + 1);
    std::string resolved_version = version;

    // If version is "latest", fetch the latest release version
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
                logger_->error("[muuk::install] Failed to fetch latest release version of {}/{}", author, repo_name);
                return;
            }
        }
        catch (const std::exception& e) {
            logger_->error("[muuk::install] Error fetching latest release: {}", e.what());
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
        logger_->info("[muuk::install] Downloading file from URL: {}", archive_url);
        util::download_file(archive_url, zip_path);

        logger_->info("Extracting downloaded file: {}", zip_path);
        util::extract_zip(zip_path, deps_path);

        // Detect the extracted folder
        bool renamed = false;
        for (const auto& entry : fs::directory_iterator(deps_path)) {
            if (entry.is_directory() && entry.path().filename().string().find(repo_name) == 0) {
                fs::rename(entry.path(), renamed_folder);
                logger_->info("[muuk::install] Renamed extracted folder '{}' to '{}'", entry.path().string(), renamed_folder);
                renamed = true;
                break;
            }
        }

        if (!renamed) {
            logger_->error("[muuk::install] Could not find the expected extracted folder: {}", expected_extracted_folder);
        }

        // Clean up ZIP file
        util::remove_path(zip_path);

        download_patch(author, repo_name, resolved_version);
        add_dependency(author, repo_name, resolved_version);
    }
    catch (const std::exception& e) {
        logger_->error("[muuk::install] Error downloading GitHub repository: {}", e.what());
    }
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

void Muuk::download_patch(const std::string& author, const std::string& repo_name, const std::string& version) {
    logger_->info("[muuk::patch] Downloading patch for {}-{} - Version: {}", author, repo_name, version);

    std::string patch_name = author + "-" + repo_name + "-" + version + ".toml";
    std::string patch_url = "https://github.com/evanwporter/muuk-tomls/raw/main/" + patch_name;

    std::string modules_folder = "modules";
    std::string module_folder = modules_folder + "/" + author + "-" + repo_name + "-" + version;
    std::string temp_patch_path = modules_folder + "/" + patch_name;
    std::string final_patch_path = module_folder + "/muuk.toml";

    util::ensure_directory_exists(module_folder);

    try {
        logger_->info("[muuk::patch] Downloading patch from {}", patch_url);
        util::download_file(patch_url, temp_patch_path);  // Since download_file returns void, no direct error handling

        if (!fs::exists(temp_patch_path)) {
            logger_->warn("[muuk::patch] Patch file '{}' was not found after download. Skipping.", temp_patch_path);
            return;
        }

        fs::rename(temp_patch_path, final_patch_path);
        logger_->info("[muuk::patch] Patch successfully applied to '{}'", final_patch_path);
    }
    catch (const std::exception& e) {
        logger_->error("[muuk::patch] Error downloading patch: {}", e.what());
    }
}

void Muuk::upload_patch(bool dry_run) {
    logger_->info("[muuk::patch] Scanning for patches to upload...");

    std::string modules_folder = "modules";
    std::string patch_repo_path = "muuk-tomls";
    fs::path patch_index_file = patch_repo_path + "/uploaded_patches.txt";

    if (!fs::exists(patch_repo_path)) {
        logger_->error("[muuk::patch] Patch repository directory '{}' does not exist!", patch_repo_path);
        return;
    }

    // Read already uploaded patches (stored in `uploaded_patches.txt`)
    std::unordered_set<std::string> uploaded_patches;
    if (fs::exists(patch_index_file)) {
        std::ifstream infile(patch_index_file);
        std::string line;
        while (std::getline(infile, line)) {
            uploaded_patches.insert(line);
        }
    }

    std::vector<std::string> patches_to_upload;

    for (const auto& entry : fs::directory_iterator(modules_folder)) {
        if (!fs::is_directory(entry.path())) continue;

        std::string module_name = entry.path().filename().string();
        std::string patch_file = entry.path().string() + "/muuk.toml";
        std::string patch_target = patch_repo_path + "/" + module_name + ".toml";

        // Check if the patch exists and hasn't been uploaded
        if (fs::exists(patch_file) && uploaded_patches.find(module_name) == uploaded_patches.end()) {
            patches_to_upload.push_back(module_name);
        }
    }

    // Dry Run Mode
    if (dry_run) {
        if (patches_to_upload.empty()) {
            logger_->info("[muuk::patch] No patches need to be uploaded.");
        }
        else {
            logger_->info("[muuk::patch] The following patches would be uploaded:");
            for (const auto& patch : patches_to_upload) {
                logger_->info(" - {}", patch);
            }
        }
        return;
    }

    // Actual Upload
    for (const auto& module_name : patches_to_upload) {
        logger_->info("[muuk::patch] Uploading patch for module '{}'", module_name);
        std::string patch_file = modules_folder + "/" + module_name + "/muuk.toml";
        std::string patch_target = patch_repo_path + "/" + module_name + ".toml";

        try {
            // Copy patch to repo
            fs::copy(patch_file, patch_target, fs::copy_options::overwrite_existing);
            logger_->info("[muuk::patch] Patch copied to '{}'", patch_target);

            // Git commit and push
            std::string git_add = "cd " + patch_repo_path + " && git add " + patch_target;
            std::string git_commit = "cd " + patch_repo_path + " && git commit -m 'Added patch for " + module_name + "'";
            std::string git_push = "cd " + patch_repo_path + " && git push origin main";

            util::execute_command(git_add.c_str());
            util::execute_command(git_commit.c_str());
            util::execute_command(git_push.c_str());

            // Record uploaded patch
            std::ofstream outfile(patch_index_file, std::ios::app);
            outfile << module_name << "\n";
            outfile.close();

            logger_->info("[muuk::patch] Patch for '{}' uploaded successfully.", module_name);
        }
        catch (const std::exception& e) {
            logger_->error("[muuk::patch] Error uploading patch '{}': {}", module_name, e.what());
        }
    }
}

void Muuk::install_submodule(const std::string& repo) {
    logger_->info("[muuk::install] Installing Git submodule: {}", repo);

    size_t slash_pos = repo.find('/');
    if (slash_pos == std::string::npos) {
        logger_->error("[muuk::install] Invalid repository format. Expected <author>/<repo> but got: {}", repo);
        return;
    }

    std::string repo_name = repo.substr(slash_pos + 1);
    std::string submodule_path = "deps/" + repo_name;

    util::ensure_directory_exists("deps");

    std::string git_command = "git submodule add https://github.com/" + repo + ".git " + submodule_path;
    logger_->info("[muuk::install] Executing: {}", git_command);

    int result = util::execute_command(git_command.c_str());
    if (result != 0) {
        logger_->error("[muuk::install] Failed to add submodule '{}'.", repo);
        return;
    }

    std::string init_command = "git submodule update --init --recursive " + submodule_path;
    logger_->info("[muuk::install] Initializing submodule...");
    result = util::execute_command(init_command.c_str());

    if (result != 0) {
        logger_->error("[muuk::install] Failed to initialize submodule '{}'.", repo);
        return;
    }

    logger_->info("[muuk::install] Successfully added and initialized submodule at '{}'.", submodule_path);
}
