#include "muukbuilder.h"
#include "util.h"
#include "logger.h"
#include "muukfiler.h"


MuukBuilder::MuukBuilder(MuukFiler& config_manager)
    : config_manager_(config_manager)
{
    logger_ = logger::get_logger("muukbuilder_logger");
}

void MuukBuilder::build(
    bool is_release, 
    std::string& target_build, 
    const std::string& compiler, 
    const std::string& profile
) {
    std::string build_type = is_release ? "release" : "debug";
    logger_->info("Generating lockfile...");

    lock_generator_ = std::make_unique<MuukLockGenerator>("./");

    lock_generator_->generate_lockfile("muuk.lock.toml", is_release);

    // Detect compiler, archiver & linker
    std::string selected_compiler = compiler.empty() ? detect_default_compiler() : compiler;
    std::string selected_archiver = detect_archiver(selected_compiler);
    std::string selected_linker = detect_linker(selected_compiler);

    ninja_generator_ = std::make_unique<NinjaGenerator>(
        "muuk.lock.toml", 
        build_type, 
        selected_compiler, 
        selected_archiver, 
        selected_linker
    );
    ninja_generator_->generate_ninja_files(target_build);

    logger_->info("Starting Ninja build...");

    execute_build(is_release);
}

void MuukBuilder::execute_build(bool is_release) const {
    std::string build_type = is_release ? "Release" : "Debug";

    // Use Ninja to execute the build
    std::string ninja_command = "ninja";
    logger_->info("Running {} build: {}", build_type, ninja_command);

    int result = util::execute_command(ninja_command.c_str());

    if (result != 0) {
        logger_->error("{} build failed with error code: {}", build_type, result);
    }
    else {
        logger_->info("{} build completed successfully.", build_type);

        std::string compdb_command = "ninja -t compdb > build/compile_commands.json";
        logger_->info("Generating compile_commands.json...");
        int compdb_result = util::execute_command(compdb_command.c_str());

        if (compdb_result != 0) {
            logger_->error("Failed to generate compile_commands.json");
        }
        else {
            logger_->info("Successfully generated compile_commands.json");
        }
    }
}

bool MuukBuilder::is_compiler_available() const {
    const char* compilers[] = { "cl", "gcc", "c++", "g++", "clang++" };

    for (const char* compiler : compilers) {
        if (util::command_exists(compiler)) {
            logger_->info("Found compiler: {}", compiler);
            return true;
        }
    }

    logger_->error("No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
    return false;
}

std::string MuukBuilder::detect_archiver(const std::string& compiler) const {
    #ifdef _WIN32
    if (compiler == "cl") return "lib";  // MSVC
    if (compiler.find("clang") != std::string::npos) return "llvm-ar"; // Clang
    return "ar"; // MinGW (GCC)
    #else
    // (void)compiler;
    if (compiler == "clang++") return "llvm-ar"; // Use `llvm-ar` with Clang
    return "ar"; // Default for Linux/macOS GCC
    #endif
}
    
    std::string MuukBuilder::detect_linker(const std::string& compiler) const {
    #ifdef _WIN32
        if (compiler == "cl") return "link"; // MSVC
        return compiler; // MinGW & Clang use the same compiler for linking
    #else
        return compiler; // On Linux/macOS, the compiler also acts as the linker
    #endif
}
    

std::string MuukBuilder::detect_default_compiler() const {
    const char* compilers[] = { "g++", "clang++", "cl" };

    for (const char* compiler : compilers) {
        if (util::command_exists(compiler)) {
            logger_->info("Found default compiler: {}", compiler);
            return compiler;
        }
    }

    logger_->error("No suitable C++ compiler found. Install GCC, Clang, or MSVC.");
    return "g++";  // Default fallback
}
    