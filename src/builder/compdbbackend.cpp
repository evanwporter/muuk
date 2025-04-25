#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "buildbackend.hpp"
#include "buildmanager.hpp"
#include "buildparser.hpp"
#include "buildtargets.hpp"
#include "logger.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// make sure it only generates profile specific stuff

CompileCommandsBackend::CompileCommandsBackend(
    BuildManager& build_manager,
    muuk::Compiler compiler,
    const std::string& archiver,
    const std::string& linker) :
    BuildBackend(build_manager, compiler, archiver, linker) { }

void CompileCommandsBackend::generate_build_file(const std::string& target_build, const std::string& profile) {
    (void)target_build;
    muuk::logger::info("");
    muuk::logger::info("  Generating compile_commands.json for '{}'", profile);
    muuk::logger::info("----------------------------------------------");

    build_dir_ = fs::path("build") / profile;
    if (!fs::exists(build_dir_)) {
        fs::create_directories(build_dir_);
        muuk::logger::info("Created build directory: {}", build_dir_.string());
    }

    auto [profile_cflags, profile_lflags] = get_profile_flag_strings(build_manager, profile);

    // Generate compile_commands.json
    json compile_commands = generate_compile_commands(profile_cflags);

    std::ofstream out(build_dir_ / "compile_commands.json");
    if (!out) {
        throw std::runtime_error("Failed to create compile_commands.json");
    }

    out << compile_commands.dump(4);
    out.close();

    muuk::logger::info("compile_commands.json generated successfully!");
}

json CompileCommandsBackend::generate_compile_commands(const std::string& profile_cflags) {
    json compile_commands = json::array();

    for (const auto& target : build_manager.get_compilation_targets()) {
        json entry;
        entry["directory"] = fs::absolute(build_dir_).string();
        entry["file"] = target.inputs[0];
        entry["output"] = target.output;

        std::ostringstream command;
        command << compiler_.to_string() << " -c " << target.inputs[0] << " -o " << target.output;

        if (!profile_cflags.empty()) {
            command << " " << profile_cflags;
        }

        for (const auto& flag : target.flags) {
            command << " " << flag;
        }

        entry["command"] = command.str();
        compile_commands.push_back(entry);
    }

    return compile_commands;
}
