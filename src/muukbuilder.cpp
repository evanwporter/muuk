#include "../include/muukbuilder.h"
#include "../include/util.h"
#include "../include/logger.h"


MuukBuilder::MuukBuilder(IMuukFiler& config_manager)
    : config_manager_(config_manager),
    lock_generator_("modules", "muuk.lock.toml", "muuk.toml"),
    ninja_generator_() {
    logger_ = Logger::get_logger("muukbuilder_logger");
}

void MuukBuilder::build(const std::vector<std::string>& args, bool is_release) {

    std::string build_type = is_release ? "release" : "debug";
    logger_->info("[muukbuilder::build] Generating lockfile...");

    lock_generator_.generateLockFile();

    logger_->info("[muukbuilder::build] Generating Ninja build script...");

    ninja_generator_.GenerateNinjaFile("muuk.lock.toml");

    logger_->info("[muukbuilder::build] Starting Ninja build...");

    execute_build(is_release);
}

void MuukBuilder::execute_build(bool is_release) const {
    std::string build_type = is_release ? "Release" : "Debug";

    // **Use Ninja to execute the build**
    std::string ninja_command = "ninja";
    logger_->info("[muukbuilder::build] Running {} build: {}", build_type, ninja_command);

    int result = util::execute_command(ninja_command.c_str());

    if (result != 0) {
        logger_->error("[muukbuilder::build] {} build failed with error code: {}", build_type, result);
    }
    else {
        logger_->info("[muukbuilder::build] {} build completed successfully.", build_type);
    }
}

bool MuukBuilder::is_compiler_available() const {
    const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

    for (const char* compiler : compilers) {
        std::string check_command = std::string(compiler) + " --version >nul 2>&1";
        if (util::execute_command(check_command.c_str()) == 0) {
            logger_->info("[muukbuilder::check] Found compiler: {}", compiler);
            return true;
        }
    }

    logger_->error("[muukbuilder::check] No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
    return false;
}