#include "muukbuilder.h"
#include "util.h"
#include "logger.h"
#include "muukfiler.h"
#include "muuk.h"

#include <string>
#include <stdexcept>

namespace muuk::compiler {
    std::string to_string(Compiler compiler) {
        switch (compiler) {
        case Compiler::GCC: return "g++";
        case Compiler::Clang: return "clang++";
        case Compiler::MSVC: return "cl";
        }
        throw std::invalid_argument("Invalid compiler type");
    }

    Compiler from_string(const std::string& compiler_str) {
        if (compiler_str == "g++" || compiler_str == "gcc") return Compiler::GCC;
        if (compiler_str == "clang++" || compiler_str == "clang") return Compiler::Clang;
        if (compiler_str == "cl" || compiler_str == "msvc") return Compiler::MSVC;
        throw std::invalid_argument("Unsupported compiler: " + compiler_str);
    }
}

MuukBuilder::MuukBuilder(MuukFiler& config_manager)
    : config_manager_(config_manager)
{
}

void MuukBuilder::build(std::string& target_build, const std::string& compiler, const std::string& profile) {
    // muuk::logger::info("Generating lockfile...");

    lock_generator_ = std::make_unique<MuukLockGenerator>("./");
    lock_generator_->generate_lockfile("muuk.lock.toml");
    spdlog::default_logger()->flush();

    muuk::compiler::Compiler selected_compiler = compiler.empty() ? detect_default_compiler() : muuk::compiler::from_string(compiler);
    std::string selected_archiver = detect_archiver(selected_compiler);
    std::string selected_linker = detect_linker(selected_compiler);

    std::string selected_profile = profile;

    if (selected_profile.empty()) {
        auto config = config_manager_.get_config();
        if (!config.contains("profile") || !config["profile"].is_table()) {
            muuk::logger::error("No profiles found in 'muuk.lock.toml'. Exiting...");
            return;
        }

        auto profiles_table = config["profile"].as_table();

        for (const auto& [profile_name, profile_data] : *profiles_table) {
            if (profile_data.is_table()) {
                auto profile_table = profile_data.as_table();
                if (
                    profile_table->contains("default") &&
                    profile_table->at("default").is_boolean() &&
                    profile_table->at("default").as_boolean()
                    ) {
                    selected_profile = profile_name.str();
                    break;
                }
            }
        }

        if (selected_profile.empty()) {
            selected_profile = std::string(profiles_table->begin()->first.str());
            muuk::logger::info("No default profile specified. Using first available profile: '{}'", selected_profile);
        }

        if (!profiles_table->empty() && profiles_table->begin()->first.str() == selected_profile) {
            muuk::logger::info("Default profile detected. Adding script entry to 'muuk.toml'...");
            add_script(selected_profile);
        }
    }

    ninja_backend_ = std::make_unique<NinjaBackend>(
        selected_compiler,
        selected_archiver,
        selected_linker,
        "muuk.lock.toml"
    );

    muuk::logger::info("Generating Ninja file for '{}'", selected_profile);

    spdlog::default_logger()->flush();

    ninja_backend_->generate_build_file(target_build, selected_profile);
    execute_build(selected_profile);
}

void MuukBuilder::execute_build(const std::string& profile) const {
    muuk::logger::info("Starting build for profile: {}", profile);

    std::string build_dir = "build/" + profile;
    std::string ninja_command = /*"cd " + build_dir + " && */"ninja -C" + build_dir;
    muuk::logger::info("Running Ninja build: {}", ninja_command);

    int result = util::execute_command(ninja_command.c_str());

    if (result != 0) {
        muuk::logger::error("Build for profile '{}' failed with error code: {}", profile, result);
    }
    else {
        muuk::logger::info("Build for profile '{}' completed successfully.", profile);

        std::string compdb_command = "cd " + build_dir + " && ninja -t compdb > compile_commands.json";
        muuk::logger::info("Generating compile_commands.json...");
        int compdb_result = util::execute_command(compdb_command.c_str());

        if (compdb_result != 0) {
            muuk::logger::error("Failed to generate compile_commands.json for profile '{}'", profile);
        }
        else {
            muuk::logger::info("Successfully generated compile_commands.json for profile '{}'", profile);
        }
    }
}

bool MuukBuilder::is_compiler_available() const {
    const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

    for (const char* compiler : compilers) {
        if (util::command_exists(compiler)) {
            muuk::logger::info("Found compiler: {}", compiler);
            return true;
        }
    }

    muuk::logger::error("No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
    return false;
}

std::string MuukBuilder::detect_archiver(muuk::compiler::Compiler compiler) const {
    switch (compiler) {
    case muuk::compiler::Compiler::MSVC: return "lib";
    case muuk::compiler::Compiler::Clang: return "llvm-ar";
    case muuk::compiler::Compiler::GCC: return "ar";
    }
    return "ar"; // Default case
}

std::string MuukBuilder::detect_linker(muuk::compiler::Compiler compiler) const {
    switch (compiler) {
    case muuk::compiler::Compiler::MSVC: return "link";
    case muuk::compiler::Compiler::Clang:
    case muuk::compiler::Compiler::GCC: return to_string(compiler); // Compiler acts as linker
    }
    return to_string(compiler); // Default case
}

muuk::compiler::Compiler MuukBuilder::detect_default_compiler() const {
    const char* compilers[] = { "g++", "clang++", "cl" };

    for (const char* compiler : compilers) {
        if (util::command_exists(compiler)) {
            muuk::logger::info("Found default compiler: {}", compiler);
            return muuk::compiler::from_string(compiler);
        }
    }

    muuk::logger::error("No suitable C++ compiler found. Install GCC, Clang, or MSVC.");
    return muuk::compiler::Compiler::GCC; // Default to GCC
}


void MuukBuilder::add_script(const std::string& profile) {
    try {
        MuukFiler muuk_filer("muuk.toml");

        std::string executable_name = "muuk";
#ifdef _WIN32
        std::string executable_path = "build/" + profile + "/" + executable_name + ".exe";
#else
        std::string executable_path = "./build/" + profile + "/" + executable_name;
#endif

        auto& scripts_section = muuk_filer.get_section("scripts");
        scripts_section.insert_or_assign("run", toml::value(executable_path));
        muuk_filer.write_to_file();

        muuk::logger::info("Successfully added run script to 'muuk.toml': {}", executable_path);
    }
    catch (const std::exception& e) {
        muuk::logger::error("Failed to add run script: {}", e.what());
    }
}

