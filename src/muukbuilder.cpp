#include "../include/muukbuilder.h"

MuukBuilder::MuukBuilder(IMuukFiler& config_manager)
    : config_manager_(config_manager),
    lock_generator_("./"),
    ninja_generator_("muuk.lock.toml", "debug") {
    logger_ = Logger::get_logger("muukbuilder_logger");
}

void MuukBuilder::build(bool is_release, std::string& target_build) {
    std::string build_type = is_release ? "release" : "debug";
    logger_->info("[muukbuilder::build] Generating lockfile...");

    // Generate lock file using MuukLockGenerator
    lock_generator_.parse_muuk_toml("muuk.toml", true);

    for (const auto& [build_name, _] : lock_generator_.get_resolved_packages().at("build")) {
        lock_generator_.resolve_dependencies(build_name);
    }

    lock_generator_.generate_lockfile("muuk.lock.toml", is_release);

    logger_->info("[muukbuilder::build] Generating Ninja build script...");

    // Generate Ninja file using NinjaGenerator
    ninja_generator_ = NinjaGenerator("muuk.lock.toml", build_type);
    ninja_generator_.generate_ninja_file(target_build);

    logger_->info("[muukbuilder::build] Starting Ninja build...");

    execute_build(is_release);
}

void MuukBuilder::execute_build(bool is_release) const {
    std::string build_type = is_release ? "Release" : "Debug";

    // Use Ninja to execute the build
    std::string ninja_command = "ninja";
    logger_->info("[muukbuilder::build] Running {} build: {}", build_type, ninja_command);

    int result = util::execute_command(ninja_command.c_str());

    if (result != 0) {
        logger_->error("[muukbuilder::build] {} build failed with error code: {}", build_type, result);
    }
    else {
        logger_->info("[MuukBuilder::execute_build] {} build completed successfully.", build_type);

        std::string compdb_command = "ninja -t compdb > build/compile_commands.json";
        logger_->info("[MuukBuilder::execute_build] Generating compile_commands.json...");
        int compdb_result = util::execute_command(compdb_command.c_str());

        if (compdb_result != 0) {
            logger_->error("[MuukBuilder::execute_build] Failed to generate compile_commands.json");
        }
        else {
            logger_->info("[MuukBuilder::execute_build] Successfully generated compile_commands.json");
        }
    }
}

bool MuukBuilder::is_compiler_available() const {
    const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

    for (const char* compiler : compilers) {
        if (util::command_exists(compiler)) {
            logger_->info("[muukbuilder::check] Found compiler: {}", compiler);
            return true;
        }
    }

    logger_->error("[muukbuilder::check] No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
    return false;
}