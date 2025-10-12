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

#include "buildconfig.h"
#include "commands/clean.hpp"
#include "commands/run.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "util.hpp"

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

    Result<void> run_build_target(std::string& target_name, const std::string& profile) {
        using namespace std::filesystem;
        muuk::logger::info("Running target: {}", target_name);

        // Parse muuk.toml
        auto parse_result = muuk::parse_muuk_file("muuk.toml");
        if (!parse_result)
            return Err(parse_result);

        const auto& root = parse_result.value();
        if (!root.contains("build"))
            return Err("No [build] section found in muuk.toml.");

        const auto& build_table = root.at("build").as_table();

        if (target_name.empty()) {
            // Default to project name (from [package])
            if (!root.contains("package") || !root.at("package").is_table())
                return Err("No target provided and no [package] section found in muuk.toml.");

            const auto& pkg_table = root.at("package").as_table();
            if (!pkg_table.contains("name"))
                return Err("The [package] section has no 'name' field — cannot determine default target.");

            target_name = toml::find<std::string>(root.at("package"), "name");
            muuk::logger::info("No target specified. Using build target '{}'", target_name);
        }

        if (!build_table.contains(target_name))
            return Err("Build target '{}' not found in muuk.toml.", target_name);

        const auto& target = build_table.at(target_name).as_table();

        // Check profile support
        std::unordered_set<std::string> target_profiles;
        if (target.contains("profiles") && target.at("profiles").is_array()) {
            for (const auto& p : target.at("profiles").as_array())
                target_profiles.insert(p.as_string());
        } else if (target.contains("profile") && target.at("profile").is_array()) {
            for (const auto& p : target.at("profile").as_array())
                target_profiles.insert(p.as_string());
        }

        if (target_profiles.empty()) {
            muuk::logger::trace("No profile restriction for target '{}'. Assuming all profiles.", target_name);
            target_profiles = { "debug", "release" };
        }

        if (!target_profiles.contains(profile))
            return Err("Target '{}' does not support the '{}' profile.", target_name, profile);

        // const auto& profiles = target.at("profile").as_array();
        // bool has_debug = std::any_of(profiles.begin(), profiles.end(), [](const auto& val) {
        //     return val.as_string() == "debug";
        // });
        // if (!has_debug)
        //     return Err("Target '{}' does not support the 'debug' profile.", target_name);

        // Path to executable
        path exe_path = path("build") / profile / (target_name + EXE_EXT);

        if (!exists(exe_path))
            return Err("Executable for '{}' not found at '{}'. Try building first.", target_name, exe_path.string());

        // Construct command line
        std::string command = "\"" + exe_path.string() + "\"";

        muuk::logger::info("Executing command: {}", command);

        int result = util::command_line::execute_command(command);
        if (result != 0)
            muuk::logger::error("Target '{}' exited with code {}", target_name, result);
        else
            muuk::logger::info("Target '{}' executed successfully.", target_name);

        return {};
    }

} // namespace muuk