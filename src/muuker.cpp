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
}

void Muuker::clean() const {
    muuk::logger::info("Starting clean operation.");

    if (!config_manager_.has_section("clean")) {
        muuk::logger::warn("'clean' operation is not defined in the config file.");
        return;
    }

    fs::path current_dir = fs::current_path();
    muuk::logger::info("Current working directory: {}", current_dir.string());

    if (!fs::exists(current_dir)) {
        muuk::logger::warn("Skipping clean operation: Directory '{}' does not exist.", current_dir.string());
        return;
    }

    auto clean_section = config_manager_.get_section("clean");
    auto clean_patterns = clean_section.get_as<toml::array>("patterns");

    if (!clean_patterns) {
        muuk::logger::warn("'clean.patterns' is not a valid array.");
        return;
    }

    // Log detected cleaning patterns
    muuk::logger::info("Cleaning patterns:");
    for (const auto& pattern : *clean_patterns) {
        if (pattern.is_string()) {
            muuk::logger::info(" - {}", *pattern.value<std::string>());
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
                muuk::logger::info("Removing file: {}", file.string());
                util::remove_path(file.string());
            }
            catch (const std::exception& e) {
                muuk::logger::warn("Failed to remove '{}': {}", file.string(), e.what());
            }
        }
    }
    catch (const std::exception& e) {
        muuk::logger::error("Error during directory iteration: {}", e.what());
    }

    muuk::logger::info("Clean operation completed.");
}

void Muuker::run_script(const std::string& script, const std::vector<std::string>& args) const {
    muuk::logger::info("Running script: {}", script);

    const auto& config = config_manager_.get_config();
    auto scripts_section = config.get_as<toml::table>("scripts");
    if (!scripts_section || !scripts_section->contains(script)) {
        muuk::logger::error("Script '{}' not found in the config file.", script);
        return;
    }

    auto script_entry = scripts_section->get(script);
    if (!script_entry || !script_entry->is_string()) {
        muuk::logger::error("Script '{}' must be a string command in the config file.", script);
        return;
    }

    std::string command = *script_entry->value<std::string>();
    for (const auto& arg : args) {
        command += " " + arg;
    }

    muuk::logger::info("Executing command: {}", command);
    int result = util::execute_command(command.c_str());
    if (result != 0) {
        muuk::logger::error("Command failed with error code: {}", result);
    }
    else {
        muuk::logger::info("Command executed successfully.");
    }
}

void Muuker::download_patch(const std::string& author, const std::string& repo_name, const std::string& version) {
    muuk::logger::info("Downloading patch for {}-{} - Version: {}", author, repo_name, version);

    std::string patch_name = author + "-" + repo_name + "-" + version + ".toml";
    std::string patch_url = "https://github.com/evanwporter/muuk-tomls/raw/main/" + patch_name;

    std::string modules_folder = "modules";
    std::string module_folder = modules_folder + "/" + author + "-" + repo_name + "-" + version;
    std::string temp_patch_path = modules_folder + "/" + patch_name;
    std::string final_patch_path = module_folder + "/muuk.toml";

    util::ensure_directory_exists(module_folder);

    try {
        muuk::logger::info("Downloading patch from {}", patch_url);
        util::download_file(patch_url, temp_patch_path);  // Since download_file returns void, no direct error handling

        if (!fs::exists(temp_patch_path)) {
            muuk::logger::warn("Patch file '{}' was not found after download. Skipping.", temp_patch_path);
            return;
        }

        fs::rename(temp_patch_path, final_patch_path);
        muuk::logger::info("Patch successfully applied to '{}'", final_patch_path);
    }
    catch (const std::exception& e) {
        muuk::logger::error("Error downloading patch: {}", e.what());
    }
}

void Muuker::upload_patch(bool dry_run) {
    muuk::logger::info("Scanning for patches to upload...");

    std::string modules_folder = "modules";
    std::string patch_repo_path = "muuk-tomls";
    fs::path patch_index_file = patch_repo_path + "/uploaded_patches.txt";

    if (!fs::exists(patch_repo_path)) {
        muuk::logger::error("Patch repository directory '{}' does not exist!", patch_repo_path);
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
            muuk::logger::info("No patches need to be uploaded.");
        }
        else {
            muuk::logger::info("The following patches would be uploaded:");
            for (const auto& patch : patches_to_upload) {
                muuk::logger::info(" - {}", patch);
            }
        }
        return;
    }

    // Actual Upload
    for (const auto& module_name : patches_to_upload) {
        muuk::logger::info("Uploading patch for module '{}'", module_name);
        std::string patch_file = modules_folder + "/" + module_name + "/muuk.toml";
        std::string patch_target = patch_repo_path + "/" + module_name + ".toml";

        try {
            // Copy patch to repo
            fs::copy(patch_file, patch_target, fs::copy_options::overwrite_existing);
            muuk::logger::info("Patch copied to '{}'", patch_target);

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

            muuk::logger::info("Patch for '{}' uploaded successfully.", module_name);
        }
        catch (const std::exception& e) {
            muuk::logger::error("Error uploading patch '{}': {}", module_name, e.what());
        }
    }
}

void Muuker::update_muuk_toml_with_submodule(const std::string& author, const std::string& repo_name, const std::string& version) {
    muuk::logger::info("Updating muuk.toml with new submodule: {}/{} - Version: {}", author, repo_name, version);

    if (!config_manager_.has_section("library.muuk.dependencies")) {
        muuk::logger::info("Creating 'library.muuk.dependencies' section in config.");
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
    // muuk::logger::info("muuk.toml updated successfully.");
}

void Muuker::remove_package(const std::string& package_name) {
    muuk::logger::info("Attempting to remove package: {}", package_name);

    if (!config_manager_.has_section("library.muuk.dependencies")) {
        muuk::logger::error("No dependencies section found in muuk.toml.");
        return;
    }

    toml::table dependencies = config_manager_.get_section("library.muuk.dependencies");

    if (!dependencies.contains(package_name)) {
        muuk::logger::error("Package '{}' not found in dependencies.", package_name);
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
    muuk::logger::info("Removed '{}' from muuk.toml.", package_name);

    // Determine package directory
    fs::path package_dir = fs::path("deps") / package_name;

    // If it's a Git submodule, deinitialize and remove it
    if (!git_url.empty()) {
        muuk::logger::info("Detected '{}' as a Git submodule. Removing submodule...", package_name);

        std::string git_remove_submodule = "git submodule deinit -f " + package_dir.string();
        std::string git_rm_submodule = "git rm -f " + package_dir.string();
        std::string git_clean = "rm -rf .git/modules/" + package_name;

        util::execute_command(git_remove_submodule.c_str());
        util::execute_command(git_rm_submodule.c_str());
        util::execute_command(git_clean.c_str());

        muuk::logger::info("Successfully removed Git submodule '{}'.", package_name);
    }

    if (util::path_exists(package_dir.string())) {
        try {
            util::remove_path(package_dir.string());
            muuk::logger::info("Deleted package directory: {}", package_dir.string());
        }
        catch (const std::exception& e) {
            muuk::logger::error("Failed to delete directory '{}': {}", package_dir.string(), e.what());
        }
    }
    else {
        muuk::logger::warn("Package directory '{}' does not exist.", package_dir.string());
    }

    muuk::logger::info("Package '{}' removal completed.", package_name);
}

std::string Muuker::get_package_name() const {
    muuk::logger::info("Retrieving package name.");

    if (!config_manager_.has_section("package")) {
        muuk::logger::error("No 'package' section found in the configuration.");
        return "";
    }

    auto package_section = config_manager_.get_section("package");
    auto package_name = package_section.get("name");

    if (!package_name || !package_name->is_string()) {
        muuk::logger::error("'package.name' is not found or is not a valid string.");
        return "";
    }

    std::string name = *package_name->value<std::string>();
    muuk::logger::info("Found package name: {}", name);
    return name;
}
