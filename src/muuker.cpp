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

#include <filesystem>
#include <string>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "commands/clean.hpp"
#include "logger.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {
    // TODO: allow specific profiles to be cleaned
    Result<void> clean(const toml::ordered_table& config) {
        if (!config.count("profile")) {
            return Err("No [profile] section found in the config.");
        }

        const auto& profile = config.at("profile").as_table();

        std::vector<std::string> profile_keys;
        for (const auto& [key, _] : profile)
            profile_keys.push_back(key);

        for (const auto& profile_key : profile_keys)
            util::command_line::execute_command("ninja -C build/{} -t clean", profile_key);

        return {};
    }

    Result<void> run_script(const toml::value config, const std::string& script, const std::vector<std::string>& args) {
        muuk::logger::info("Running script: {}", script);

        const auto& scripts_section = config.at("scripts");
        if (!scripts_section.contains(script))
            return Err("Script '{}' not found in the config file.", script);

        const auto& script_entry = scripts_section.at(script);
        if (!script_entry.is_string())
            return Err("Script '{}' must be a string command in the config file.", script);

        std::string command = "\"" + fs::absolute(script_entry.as_string()).string() + "\"";

        // if (!fs::exists(command))
        //     return Err("Executable not found at '{}'", command);

        for (const auto& arg : args) {
            command += " " + arg;
        }

        muuk::logger::info("Executing command: {}", command);
        int result = util::command_line::execute_command(command);
        if (result != 0)
            muuk::logger::info("Command failed with error code: {}", result);
        else
            muuk::logger::info("Command executed successfully.");
        return {};
    }
} // namespace muuk