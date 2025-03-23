#include <string>

#include "buildbackend.hpp"
#include "buildconfig.h"
#include "compiler.hpp"
#include "logger.h"
#include "muukbuilder.h"
#include "muukfiler.h"
#include "muuklockgen.h"
#include "rustify.hpp"
#include "util.h"

namespace muuk {
    Result<void> execute_build(const std::string& profile) {
        muuk::logger::info("Starting build for profile: {}", profile);

        std::string build_dir = "build/" + profile;
        std::string ninja_command = "ninja -C " + build_dir;
        muuk::logger::info("Running Ninja build: {}", ninja_command);

        int result = util::command_line::execute_command(ninja_command.c_str());
        if (result != 0) {
            return Err("Build for profile '" + profile + "' failed with error code: " + std::to_string(result));
        }

        muuk::logger::info("Build for profile '{}' completed successfully.", profile);

        return {};
    }

    Result<bool> is_compiler_available() {
        const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

        for (const char* compiler : compilers) {
            if (util::command_line::command_exists(compiler)) {
                muuk::logger::info("Found compiler: {}", compiler);
                return true;
            }
        }

        return Err("No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
    }

    Result<muuk::Compiler> detect_default_compiler() {
        const char* compilers[] = { "g++", "clang++", "cl" };

        for (const char* compiler : compilers) {
            if (util::command_line::command_exists(compiler)) {
                muuk::logger::info("Found default compiler: {}", compiler);
                return muuk::Compiler::from_string(compiler);
            }
        }

        return Err("No suitable C++ compiler found. Install GCC, Clang, or MSVC.");
    }

    Result<std::string> select_profile(const std::string& profile, const MuukFiler& config) {
        if (!profile.empty())
            return profile;

        auto data = config.get_config();
        if (!data.contains("profile") || !data["profile"].is_table())
            return Err("No profiles found in lockfile.");

        auto profiles = data["profile"].as_table();
        for (const auto& [name, val] : *profiles) {
            if (val.is_table()) {
                auto tbl = val.as_table();
                if (tbl->contains("default") && tbl->at("default").is_boolean() && tbl->at("default").value_or(false))
                    return std::string(name.str());
            }
        }

        if (!profiles->empty()) {
            auto fallback = std::string(profiles->begin()->first.str());
            muuk::logger::info("No default profile. Using first available: '{}'", fallback);
            return fallback;
        }

        return Err("No valid profiles found in lockfile.");
    }

    Result<void> add_script(const std::string& profile, const std::string& build_name) {
        (void)build_name;
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

    Result<void> generate_compile_commands(const std::string& profile, const muuk::Compiler& compiler, const std::string& archiver, const std::string& linker) {
        muuk::logger::info("Generating compile_commands.json for profile '{}'", profile);
        CompileCommandsBackend backend(compiler, archiver, linker, MUUK_CACHE_FILE);
        backend.generate_build_file("compile_commands", profile);
        muuk::logger::info("compile_commands.json generated successfully.");
        return {};
    }

    Result<void> build(std::string& target_build, const std::string& compiler, const std::string& profile, MuukFiler& config) {
        auto lock_generator_ = MuukLockGenerator::create("./");
        if (!lock_generator_)
            return Err(lock_generator_.error());

        auto lock_file_result = lock_generator_->generate_cache(MUUK_CACHE_FILE);
        if (!lock_file_result)
            return Err(lock_file_result.error());

        spdlog::default_logger()->flush();

        auto compiler_result = compiler.empty()
            ? detect_default_compiler()
            : muuk::Compiler::from_string(compiler);

        if (!compiler_result)
            return Err("Error selecting compiler: " + compiler_result.error());

        const auto& selected_compiler = compiler_result.value();
        const std::string selected_archiver = compiler_result->detect_archiver();
        const std::string selected_linker = compiler_result->detect_linker();

        auto profile_result = select_profile(profile, config);
        if (!profile_result)
            return tl::unexpected(profile_result.error());

        std::string selected_profile = profile_result.value();

        NinjaBackend build_backend(
            *compiler_result,
            selected_archiver,
            selected_linker,
            MUUK_CACHE_FILE);

        muuk::logger::info("Generating Ninja file for '{}'", selected_profile);
        build_backend.generate_build_file(target_build, selected_profile);

        execute_build(selected_profile);
        generate_compile_commands(
            selected_profile,
            selected_compiler,
            selected_archiver,
            selected_linker);

        return {};
    }
} // namespace muuk