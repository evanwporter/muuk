#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "buildbackend.hpp"
#include "buildmanager.h"
#include "buildparser.hpp"
#include "buildtargets.h"
#include "logger.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// make sure it only generates profile specific stuff

CompileCommandsBackend::CompileCommandsBackend(
    muuk::Compiler compiler,
    const std::string& archiver,
    const std::string& linker,
    const std::string& lockfile_path) :
    BuildBackend(compiler, archiver, linker, lockfile_path),
    build_manager(std::make_shared<BuildManager>()) {
}

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

    build_parser = std::make_shared<BuildParser>(build_manager, muuk_filer, compiler_, build_dir_, profile);
    build_parser->parse();

    auto [profile_cflags, profile_lflags] = build_parser->extract_profile_flags(profile);

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

    for (const auto& target : build_manager->get_compilation_targets()) {
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
