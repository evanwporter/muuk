#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <unordered_map>
#include <functional>
#include <vector>
#include <argparse/argparse.hpp>
#include "../include/util.h"
#include "../include/muuk.h"
#include "../include/muukfiler.h"
#include "../include/logger.h"


namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    Logger::initialize();
    auto logger = Logger::get_logger("main_logger");

    argparse::ArgumentParser program("muuk");

    argparse::ArgumentParser clean_command("clean", "Clean the project");
    clean_command.add_argument("clean_args")
        .remaining()
        .help("Build arguments passed to the build system")
        .default_value(std::vector<std::string>{});

    argparse::ArgumentParser run_command("run", "Run a custom script");
    run_command.add_argument("script")
        .help("The script to run, as defined in muuk.json");
    run_command.add_argument("extra_args")
        .remaining()
        .help("Extra arguments passed to the command")
        .default_value(std::vector<std::string>{});

    argparse::ArgumentParser build_command("build", "Build the project");
    build_command.add_argument("build_args")
        .remaining()
        .help("Build arguments passed to the build system")
        .default_value(std::vector<std::string>{});
    build_command.add_argument("--release")
        .help("Build in release mode (optimized)")
        .flag();

    argparse::ArgumentParser download_command("install", "Download and extract a .tar.gz file");
    download_command.add_argument("url")
        .help("The URL of the .tar.gz file to download");
    download_command.add_argument("--version")
        .help("The version to download (default: latest)")
        .default_value(std::string("latest"));

    program.add_subparser(clean_command);
    program.add_subparser(run_command);
    program.add_subparser(build_command);
    program.add_subparser(download_command);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [<script> --<arg>=<value> ...]\n";
        return 1;
    }

    MuukFiler muukFiler("muuk.toml");

    Muuk muuk(muukFiler);

    MuukBuilder muukBuilder(muukFiler);

    // Command mapping
    std::unordered_map<std::string, std::function<void()>> command_map = {
        {"clean", [&]() { muuk.clean(); }},
        {"run", [&]() {
            const auto script = run_command.present<std::string>("script");
            const auto extra_args = run_command.get<std::vector<std::string>>("extra_args");

            if (!script.has_value()) {
                std::cerr << "Error: No script name provided for the 'run' command.\n";
                return;
            }
            muuk.run_script(script.value(), extra_args);
        }},
        {"build", [&]() {
            bool is_release = build_command.get<bool>("--release");
            const auto build_args = build_command.get<std::vector<std::string>>("build_args");
            muukBuilder.build(build_args, is_release);
        }},
        {"install", [&]() {
            const auto url = download_command.get<std::string>("url");
            const auto version = download_command.get<std::string>("--version");
            muuk.download_github_release(url, version);
        }}
    };

    try {
        // Parse the arguments
        program.parse_args(argc, argv);

        // Check if any known subcommand was used
        bool command_found = false;
        for (const auto& [command_name, command_handler] : command_map) {
            if (program.is_subcommand_used(command_name)) {
                command_handler();
                command_found = true;
                break;
            }
        }

        // Handle unknown command
        if (!command_found) {
            std::cerr << "Error: Unknown command. Use '--help' to see available commands.\n";
            return 1;
        }
    }
    catch (const std::runtime_error& err) {
        std::cerr << "Error: " << err.what() << "\n";
        return 1;
    }

    return 0;
}
