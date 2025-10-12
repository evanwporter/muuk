#include <filesystem>
#include <string>

#include <toml.hpp>

#include "build/backend.hpp"
#include "build/manager.hpp"
#include "build/parser.hpp"
#include "buildconfig.h"
#include "commands/build.hpp"
#include "compiler.hpp"
#include "lockgen/muuklockgen.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "toml11/types.hpp"
#include "toml_ext.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {

    Result<void> execute_build(const std::string& profile, const std::string& target_build, const std::string& jobs) {
        muuk::logger::info("Starting build for profile: {}", profile);

        std::string build_dir = "build/" + profile;
        std::string ninja_command = "ninja -C " + build_dir;
        if (!target_build.empty())
            ninja_command += " " + target_build;
        if (!jobs.empty())
            ninja_command += " -j " + jobs;

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

    bool is_default_profile(const std::string& profile, const toml::value& config) {
        if (!config.contains("profile") || !config.at("profile").is_table())
            return false;

        const auto& profiles = config.at("profile").as_table();
        if (!profiles.contains(profile))
            return false;

        if (!profiles.at(profile).contains("default"))
            return false;

        return profiles.at(profile).at("default").as_boolean();
    }

    Result<void> add_script(const std::string& profile, const std::string& build_name) {

        auto result = muuk::parse_muuk_file<toml::ordered>("muuk.toml");
        if (!result)
            return Err(result.error());

        auto config = result.value();

        if (!is_default_profile(profile, config))
            return {};

#ifdef _WIN32
        std::string executable_path = "build/"
            + profile + "/" + build_name + ".exe";
#else
        std::string executable_path = "./build/"
            + profile + "/" + build_name + ".exe";
#endif

        if (!config.contains("scripts") || !config["scripts"].is_table()) {
            config["scripts"] = toml::ordered_table {};
        }

        config["scripts"].as_table()[build_name] = executable_path;

        std::ofstream out(MUUK_TOML_FILE);
        if (!out)
            return make_error<EC::FileNotFound>(MUUK_TOML_FILE);
        out << toml::format(config);

        muuk::logger::info("Successfully added run script to 'muuk.toml': {}", executable_path);
        return {};
    }

    Result<void> generate_compile_commands(build::BuildManager& build_manager, const std::string& profile, const muuk::Compiler& compiler, const std::string& archiver, const std::string& linker) {
        muuk::logger::info("Generating compile_commands.json for profile '{}'", profile);
        build::CompileCommandsBackend backend(build_manager, compiler, archiver, linker);
        backend.generate_build_file(profile);
        return {};
    }

    Result<void> build_cmd(const std::string& target_build, const std::string& compiler, const std::string& profile, const std::string& jobs) {
        util::file_system::ensure_directory_exists("build/" + profile);

        if (!jobs.empty() && !util::is_integer(jobs))
            return Err("Invalid number of jobs specified: " + jobs);

        // TODO: Pass the config to the lock generator
        const auto dirty_result = lockgen::MuukLockGenerator::is_dirty();
        if (dirty_result.has_value() && dirty_result.value()) {
            muuk::logger::info("`muuk.lock.toml` is out of date or dirty. Regenerating...");

            auto lock_generator_ = lockgen::MuukLockGenerator::create("./");
            if (!lock_generator_)
                return Err(lock_generator_.error());

            TRYV(lock_generator_->generate_cache(MUUK_CACHE_FILE));
            TRYV(lock_generator_->generate_fingerprints());
        }

        else {
            muuk::logger::info("`muuk.lock.toml` is up to date and does not need to be regenerated");
        }

        const auto compiler_result = compiler.empty()
            ? detect_default_compiler()
            : muuk::Compiler::from_string(compiler);

        if (!compiler_result)
            return Err("Error selecting compiler: " + compiler_result.error().message);

        const auto& selected_compiler = compiler_result.value();
        const std::string selected_archiver = selected_compiler.detect_archiver();
        const std::string selected_linker = selected_compiler.detect_linker();

        auto build_manager = build::BuildManager();

        TRYV(parse(
            build_manager,
            selected_compiler,
            fs::path("build") / profile,
            profile));

        build::NinjaBackend build_backend(
            build_manager,
            selected_compiler,
            selected_archiver,
            selected_linker);

        build_backend.generate_build_file(profile);

        execute_build(profile, target_build, jobs);
        generate_compile_commands(
            build_manager,
            profile,
            selected_compiler,
            selected_archiver,
            selected_linker);

        return {};
    }

} // namespace muuk
