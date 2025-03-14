#include <string>

#include <tl/expected.hpp>

#include "logger.h"
#include "muukbuilder.h"
#include "muukfiler.h"
#include "rustify.hpp"
#include "util.h"

MuukBuilder::MuukBuilder(MuukFiler& config_manager) :
    config_manager_(config_manager) {
}

Result<void> MuukBuilder::build(std::string& target_build, const std::string& compiler, const std::string& profile) {
    lock_generator_ = std::make_unique<MuukLockGenerator>("./");
    lock_generator_->generate_lockfile("muuk.lock.toml");
    spdlog::default_logger()->flush();

    auto compiler_result = compiler.empty() ? detect_default_compiler() : muuk::Compiler::from_string(compiler);
    if (!compiler_result)
        return tl::unexpected("Error selecting compiler: " + compiler_result.error());
    auto selected_compiler = compiler_result.value();

    std::string selected_archiver = selected_compiler.detect_archiver();
    std::string selected_linker = selected_compiler.detect_linker();

    auto profile_result = select_profile(profile);
    if (!profile_result)
        return tl::unexpected(profile_result.error());

    std::string selected_profile = *profile_result;

    ninja_backend_ = std::make_unique<NinjaBackend>(
        *compiler_result,
        selected_archiver,
        selected_linker,
        "muuk.lock.toml");

    muuk::logger::info("Generating Ninja file for '{}'", selected_profile);
    spdlog::default_logger()->flush();

    ninja_backend_->generate_build_file(target_build, selected_profile);
    execute_build(selected_profile);
    return {};
}

tl::expected<void, std::string> MuukBuilder::execute_build(const std::string& profile) const {
    muuk::logger::info("Starting build for profile: {}", profile);

    std::string build_dir = "build/" + profile;
    std::string ninja_command = "ninja -C " + build_dir;
    muuk::logger::info("Running Ninja build: {}", ninja_command);

    int result = util::command_line::execute_command(ninja_command.c_str());
    if (result != 0) {
        return tl::unexpected("Build for profile '" + profile + "' failed with error code: " + std::to_string(result));
    }

    muuk::logger::info("Build for profile '{}' completed successfully.", profile);

    std::string compdb_command = "cd " + build_dir + " && ninja -t compdb > compile_commands.json";
    muuk::logger::info("Generating compile_commands.json...");

    int compdb_result = util::command_line::execute_command(compdb_command.c_str());
    if (compdb_result != 0) {
        return tl::unexpected("Failed to generate compile_commands.json for profile '" + profile + "'");
    }

    muuk::logger::info("Successfully generated compile_commands.json for profile '{}'", profile);
    return {};
}

tl::expected<bool, std::string> MuukBuilder::is_compiler_available() const {
    const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

    for (const char* compiler : compilers) {
        if (util::command_line::command_exists(compiler)) {
            muuk::logger::info("Found compiler: {}", compiler);
            return true;
        }
    }

    return tl::unexpected("No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
}

tl::expected<muuk::Compiler, std::string> MuukBuilder::detect_default_compiler() const {
    const char* compilers[] = { "g++", "clang++", "cl" };

    for (const char* compiler : compilers) {
        if (util::command_line::command_exists(compiler)) {
            muuk::logger::info("Found default compiler: {}", compiler);
            return muuk::Compiler::from_string(compiler);
        }
    }

    return tl::unexpected("No suitable C++ compiler found. Install GCC, Clang, or MSVC.");
}

tl::expected<std::string, std::string> MuukBuilder::select_profile(const std::string& profile) {
    if (!profile.empty())
        return profile;

    auto config = config_manager_.get_config();
    if (!config.contains("profile") || !config["profile"].is_table()) {
        return tl::unexpected("No profiles found in 'muuk.lock.toml'.");
    }

    auto profiles_table = config["profile"].as_table();
    for (const auto& [profile_name, profile_data] : *profiles_table) {
        if (profile_data.is_table()) {
            auto profile_table = profile_data.as_table();
            if (profile_table->contains("default") && profile_table->at("default").is_boolean() && profile_table->at("default").as_boolean()) {
                return std::string(profile_name.str());
            }
        }
    }

    if (!profiles_table->empty()) {
        muuk::logger::info("No default profile specified. Using first available profile: '{}'", profiles_table->begin()->first.str());
        return std::string(profiles_table->begin()->first.str());
    }

    return tl::unexpected("No valid profiles found.");
}

tl::expected<void, std::string> MuukBuilder::add_script(const std::string& profile, const std::string& build_name) {
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
        return {};
    } catch (const std::exception& e) {
        return tl::unexpected("Failed to add run script: " + std::string(e.what()));
    }
}
