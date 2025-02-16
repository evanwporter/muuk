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

#include "../include/muuker.hpp"
#include "../include/muukfiler.h"
#include "../include/util.h"
#include "../include/logger.h"
#include "../include/buildconfig.h"

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <glob/glob.h>
#include <unordered_set>
#include <format>

namespace fs = std::filesystem;

// namespace Muuk {
Muuker::Muuker(MuukFiler& config_manager) : config_manager_(config_manager), muuk_builder_(config_manager) {
    logger_ = Logger::get_logger("muuk_logger");
}

void Muuker::clean() const {
    logger_->info("Starting clean operation.");

    if (!config_manager_.has_section("clean")) {
        logger_->warn("'clean' operation is not defined in the config file.");
        return;
    }

    fs::path current_dir = fs::current_path();
    logger_->info("Current working directory: {}", current_dir.string());

    if (!fs::exists(current_dir)) {
        logger_->warn("Skipping clean operation: Directory '{}' does not exist.", current_dir.string());
        return;
    }

    auto clean_section = config_manager_.get_section("clean");
    auto clean_patterns = clean_section.get_as<toml::array>("patterns");

    if (!clean_patterns) {
        logger_->warn("'clean.patterns' is not a valid array.");
        return;
    }

    // Log detected cleaning patterns
    logger_->info("Cleaning patterns:");
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

                // Ensure files_to_delete is also std::vector<std::filesystem::path>
                for (const auto& file_path : matched_files) {
                    files_to_delete.push_back(file_path);
                }
            }
        }

        // Delete collected files
        for (const auto& file : files_to_delete) {
            try {
                logger_->info("Removing file: {}", file.string());
                util::remove_path(file.string());
            }
            catch (const std::exception& e) {
                logger_->warn("Failed to remove '{}': {}", file.string(), e.what());
            }
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error during directory iteration: {}", e.what());
    }

    logger_->info("Clean operation completed.");
}

void Muuker::run_script(const std::string& script, const std::vector<std::string>& args) const {
    logger_->info("Running script: {}", script);

    const auto& config = config_manager_.get_config();
    auto scripts_section = config.get_as<toml::table>("scripts");
    if (!scripts_section || !scripts_section->contains(script)) {
        logger_->error("Script '{}' not found in the config file.", script);
        return;
    }

    auto script_entry = scripts_section->get(script);
    if (!script_entry || !script_entry->is_string()) {
        logger_->error("Script '{}' must be a string command in the config file.", script);
        return;
    }

    std::string command = *script_entry->value<std::string>();
    for (const auto& arg : args) {
        command += " " + arg;
    }

    logger_->info("Executing command: {}", command);
    int result = util::execute_command(command.c_str());
    if (result != 0) {
        logger_->error("Command failed with error code: {}", result);
    }
    else {
        logger_->info("Command executed successfully.");
    }
}

void Muuker::download_github_release(const std::string& repo, const std::string& version) {
    logger_->info("Downloading GitHub release. Repository: {}, Version: {}", repo, version);

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
        util::download_file(archive_url, zip_path);

        logger_->info("Extracting downloaded file: {}", zip_path);
        util::extract_zip(zip_path, deps_path);

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

        download_patch(author, repo_name, resolved_version);
        add_dependency(author, repo_name, resolved_version);
    }
    catch (const std::exception& e) {
        logger_->error("Error downloading GitHub repository: {}", e.what());
    }
}


void Muuker::add_dependency(const std::string& author, const std::string& repo_name, const std::string& version) {
    logger_->info("Adding/updating dependency: {}-{} - Version: {}", author, repo_name, version);

    if (!config_manager_.has_section("dependencies")) {
        logger_->info("Creating 'dependencies' section in config.");
        // config_manager_.update_section("dependencies", toml::table{});
    }

    toml::table dependencies = config_manager_.get_section("dependencies");
    std::string key = author + "-" + repo_name;

    auto existing_version = dependencies.get(key);
    if (existing_version && existing_version->is_string()) {
        if (*existing_version->value<std::string>() == version) {
            logger_->info("Dependency {} is already at version {}.", key, version);
        }
        else {
            dependencies.insert_or_assign(key, version);
            logger_->info("Updated dependency {} to version {}.", key, version);
        }
    }
    else {
        dependencies.insert_or_assign(key, version);
        logger_->info("Added new dependency: {} - Version: {}", key, version);
    }

    // config_manager_.update_section("dependencies", dependencies);
    logger_->info("Dependencies updated successfully.");
}

void Muuker::download_patch(const std::string& author, const std::string& repo_name, const std::string& version) {
    logger_->info("Downloading patch for {}-{} - Version: {}", author, repo_name, version);

    std::string patch_name = author + "-" + repo_name + "-" + version + ".toml";
    std::string patch_url = "https://github.com/evanwporter/muuk-tomls/raw/main/" + patch_name;

    std::string modules_folder = "modules";
    std::string module_folder = modules_folder + "/" + author + "-" + repo_name + "-" + version;
    std::string temp_patch_path = modules_folder + "/" + patch_name;
    std::string final_patch_path = module_folder + "/muuk.toml";

    util::ensure_directory_exists(module_folder);

    try {
        logger_->info("Downloading patch from {}", patch_url);
        util::download_file(patch_url, temp_patch_path);  // Since download_file returns void, no direct error handling

        if (!fs::exists(temp_patch_path)) {
            logger_->warn("Patch file '{}' was not found after download. Skipping.", temp_patch_path);
            return;
        }

        fs::rename(temp_patch_path, final_patch_path);
        logger_->info("Patch successfully applied to '{}'", final_patch_path);
    }
    catch (const std::exception& e) {
        logger_->error("Error downloading patch: {}", e.what());
    }
}

void Muuker::upload_patch(bool dry_run) {
    logger_->info("Scanning for patches to upload...");

    std::string modules_folder = "modules";
    std::string patch_repo_path = "muuk-tomls";
    fs::path patch_index_file = patch_repo_path + "/uploaded_patches.txt";

    if (!fs::exists(patch_repo_path)) {
        logger_->error("Patch repository directory '{}' does not exist!", patch_repo_path);
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
            logger_->info("No patches need to be uploaded.");
        }
        else {
            logger_->info("The following patches would be uploaded:");
            for (const auto& patch : patches_to_upload) {
                logger_->info(" - {}", patch);
            }
        }
        return;
    }

    // Actual Upload
    for (const auto& module_name : patches_to_upload) {
        logger_->info("Uploading patch for module '{}'", module_name);
        std::string patch_file = modules_folder + "/" + module_name + "/muuk.toml";
        std::string patch_target = patch_repo_path + "/" + module_name + ".toml";

        try {
            // Copy patch to repo
            fs::copy(patch_file, patch_target, fs::copy_options::overwrite_existing);
            logger_->info("Patch copied to '{}'", patch_target);

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

            logger_->info("Patch for '{}' uploaded successfully.", module_name);
        }
        catch (const std::exception& e) {
            logger_->error("Error uploading patch '{}': {}", module_name, e.what());
        }
    }
}

void Muuker::install_submodule(const std::string& repo) {
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

    // Update muuk.toml
    update_muuk_toml_with_submodule(author, repo_name, version);

    logger_->info("Successfully added and initialized submodule at '{}'.", submodule_path);
}


void Muuker::update_muuk_toml_with_submodule(const std::string& author, const std::string& repo_name, const std::string& version) {
    logger_->info("Updating muuk.toml with new submodule: {}/{} - Version: {}", author, repo_name, version);

    if (!config_manager_.has_section("library.muuk.dependencies")) {
        logger_->info("Creating 'library.muuk.dependencies' section in config.");
        // config_manager_.update_section("library.muuk.dependencies", toml::table{});
    }

    toml::table dependencies = config_manager_.get_section("library.muuk.dependencies");
    std::string key = repo_name;
    std::string muuk_path = "deps/" + repo_name + ".muuk.toml";
    std::string git_url = "https://github.com/" + author + "/" + repo_name + ".git";

    toml::table dependency_entry;
    dependency_entry.insert_or_assign("version", version);
    dependency_entry.insert_or_assign("git", git_url);
    dependency_entry.insert_or_assign("muuk_path", muuk_path);

    dependencies.insert_or_assign(key, dependency_entry);

    // config_manager_.update_section("library.muuk.dependencies", dependencies);
    // logger_->info("muuk.toml updated successfully.");
}

void Muuker::remove_package(const std::string& package_name) {
    logger_->info("Attempting to remove package: {}", package_name);

    if (!config_manager_.has_section("library.muuk.dependencies")) {
        logger_->error("No dependencies section found in muuk.toml.");
        return;
    }

    toml::table dependencies = config_manager_.get_section("library.muuk.dependencies");

    if (!dependencies.contains(package_name)) {
        logger_->error("Package '{}' not found in dependencies.", package_name);
        return;
    }

    auto package_entry = dependencies.get_as<toml::table>(package_name);
    std::string muuk_path, git_url;
    if (package_entry) {
        if (package_entry->contains("muuk_path")) {
            muuk_path = package_entry->get("muuk_path")->value_or("");
        }
        if (package_entry->contains("git")) {
            git_url = package_entry->get("git")->value_or("");
        }
    }

    dependencies.erase(package_name);
    // config_manager_.update_section("library.muuk.dependencies", dependencies);
    logger_->info("Removed '{}' from muuk.toml.", package_name);

    // Determine package directory
    fs::path package_dir = fs::path("deps") / package_name;

    // If it's a Git submodule, deinitialize and remove it
    if (!git_url.empty()) {
        logger_->info("Detected '{}' as a Git submodule. Removing submodule...", package_name);

        std::string git_remove_submodule = "git submodule deinit -f " + package_dir.string();
        std::string git_rm_submodule = "git rm -f " + package_dir.string();
        std::string git_clean = "rm -rf .git/modules/" + package_name;

        util::execute_command(git_remove_submodule.c_str());
        util::execute_command(git_rm_submodule.c_str());
        util::execute_command(git_clean.c_str());

        logger_->info("Successfully removed Git submodule '{}'.", package_name);
    }

    if (util::path_exists(package_dir.string())) {
        try {
            util::remove_path(package_dir.string());
            logger_->info("Deleted package directory: {}", package_dir.string());
        }
        catch (const std::exception& e) {
            logger_->error("Failed to delete directory '{}': {}", package_dir.string(), e.what());
        }
    }
    else {
        logger_->warn("Package directory '{}' does not exist.", package_dir.string());
    }

    logger_->info("Package '{}' removal completed.", package_name);
}

std::string Muuker::get_package_name() const {
    logger_->info("Retrieving package name.");

    if (!config_manager_.has_section("package")) {
        logger_->error("No 'package' section found in the configuration.");
        return "";
    }

    auto package_section = config_manager_.get_section("package");
    auto package_name = package_section.get("name");

    if (!package_name || !package_name->is_string()) {
        logger_->error("'package.name' is not found or is not a valid string.");
        return "";
    }

    std::string name = *package_name->value<std::string>();
    logger_->info("Found package name: {}", name);
    return name;
}

void Muuker::init_project() {
    logger_->info("Initializing a new muuk.toml configuration...");

    std::string project_name, author, version, license, include_path;

    std::cout << "Enter project name: ";
    std::getline(std::cin, project_name);

    std::cout << "Enter author name: ";
    std::getline(std::cin, author);

    std::cout << "Enter project version (default: 0.1.0): ";
    std::getline(std::cin, version);
    if (version.empty()) version = "0.1.0";

    std::cout << "Enter license (e.g., MIT, GPL, Apache, Unlicensed, default: MIT): ";
    std::getline(std::cin, license);
    if (license.empty()) license = "MIT";

    std::cout << "Enter include path (default: include/): ";
    std::getline(std::cin, include_path);
    if (include_path.empty()) include_path = "include/";

    toml::table config;
    config.insert_or_assign("package", toml::table{
        {"name", project_name},
        {"author", author},
        {"version", version},
        {"license", license}
        });

    config.insert_or_assign("build", toml::table{
        {"include_path", include_path}
        });

    config.insert_or_assign("dependencies", toml::table{});

    std::ofstream config_file("muuk.toml");
    if (!config_file) {
        logger_->error("Failed to create muuk.toml.");
        return;
    }

    config_file << config;
    config_file.close();

    logger_->info("Successfully created muuk.toml!");
    std::cout << "\nSuccessfully initialized muuk project!\n";
}

void Muuker::generate_license(const std::string& license, const std::string& author) {
    std::string license_text;

    if (license == "MIT") {
        license_text = std::format(R"(MIT License

Copyright (c) {0} {1}

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.)", util::current_year(), author);
    }
    else if (license == "GPL") {
        license_text = "GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007...";
    }
    else {
        license_text = std::format(R"(Unlicensed

All rights reserved. {0} {1} reserves all rights to the software.
)", util::current_year(), author);
    }

    std::ofstream license_file("LICENSE");
    if (license_file) {
        license_file << license_text;
        logger_->info("[muuk] Generated LICENSE file.");
    }
}
