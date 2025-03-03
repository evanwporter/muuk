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

namespace muuk {
    void clean(MuukFiler& config_manager_) {
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

        try {
            std::vector<fs::path> files_to_delete;

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

    void run_script(const MuukFiler& config_manager_, const std::string& script, const std::vector<std::string>& args) {
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
        int result = util::execute_command(command);
        if (result != 0) {
            muuk::logger::error("Command failed with error code: {}", result);
        }
        else {
            muuk::logger::info("Command executed successfully.");
        }
    }

}