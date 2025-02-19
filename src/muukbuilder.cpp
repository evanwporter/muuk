#include "muukbuilder.h"
#include "util.h"
#include "logger.h"
#include "muukfiler.h"


MuukBuilder::MuukBuilder(MuukFiler& config_manager)
    : config_manager_(config_manager)
{
    logger_ = logger::get_logger("muukbuilder_logger");
}

void MuukBuilder::build(std::string& target_build, const std::string& compiler, const std::string& profile) {
    logger_->info("Generating lockfile...");

    lock_generator_ = std::make_unique<MuukLockGenerator>("./");
    lock_generator_->generate_lockfile("muuk.lock.toml");

    std::string selected_compiler = compiler.empty() ? detect_default_compiler() : compiler;
    std::string selected_archiver = detect_archiver(selected_compiler);
    std::string selected_linker = detect_linker(selected_compiler);

    std::string selected_profile = profile;

    // TODO: Allow specifying default profile
    if (selected_profile.empty()) {
        auto config = config_manager_.get_config();
        if (!config.contains("profile") || !config["profile"].is_table()) {
            logger::error("No profiles found in 'muuk.lock.toml'. Exiting...");
            return;
        }

        auto profiles_table = config["profile"].as_table();
        selected_profile = std::string(profiles_table->begin()->first.str());
        logger_->info("No profile specified. Using first available profile: '{}'", selected_profile);

        if (!profiles_table->empty() && profiles_table->begin()->first.str() == selected_profile) {
            logger_->info("Default profile detected. Adding script entry to 'muuk.toml'...");
            add_script(selected_profile);
        }
    }

    ninja_generator_ = std::make_unique<NinjaGenerator>(
        "muuk.lock.toml",
        selected_profile,
        selected_compiler,
        selected_archiver,
        selected_linker
    );

    ninja_generator_->generate_ninja_file(selected_profile, target_build);
    execute_build(selected_profile);
}

void MuukBuilder::execute_build(const std::string& profile) const {
    logger_->info("Starting build for profile: {}", profile);

    std::string ninja_command = "ninja -f build/" + profile + ".ninja";
    logger_->info("Running Ninja build: {}", ninja_command);

    int result = util::execute_command(ninja_command.c_str());

    if (result != 0) {
        logger::error("Build for profile '{}' failed with error code: {}", profile, result);
    }
    else {
        logger_->info("Build for profile '{}' completed successfully.", profile);

        std::string compdb_command = "ninja -f build/" + profile + ".ninja " + " -t compdb > build/" + profile + "/compile_commands.json";
        logger_->info("Generating compile_commands.json...");
        int compdb_result = util::execute_command(compdb_command.c_str());

        if (compdb_result != 0) {
            logger::error("Failed to generate compile_commands.json for profile '{}'", profile);
        }
        else {
            logger_->info("Successfully generated compile_commands.json for profile '{}'", profile);
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

    logger::error("No compatible C++ compiler found on PATH. Install MSVC, GCC, or Clang.");
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

    logger::error("No suitable C++ compiler found. Install GCC, Clang, or MSVC.");
    return "g++";
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

        logger_->info("Successfully added run script to 'muuk.toml': {}", executable_path);
    }
    catch (const std::exception& e) {
        logger::error("Failed to add run script: {}", e.what());
    }
}

