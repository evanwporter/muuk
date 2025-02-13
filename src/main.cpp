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
#include <cracklib.h>
#include "../include/util.h"
#include "../include/muuk.h"
#include "../include/muukfiler.h"
#include "../include/logger.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    Logger::initialize();
    auto logger = Logger::get_logger("main_logger");

    argparse::ArgumentParser program("muuk");

    program.add_argument("--muuk-path")
        .help("Specify the path to muuk.toml")
        .default_value(std::string("muuk.toml"));

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

    argparse::ArgumentParser download_command("install", "Install a package from github");
    download_command.add_argument("url")
        .help("The author and zip file of the github repo to install");
    download_command.add_argument("--version")
        .help("The version to download (default: latest)")
        .default_value(std::string("latest"));

    argparse::ArgumentParser upload_patch_command("upload-patch", "Upload missing patches.");
    upload_patch_command.add_argument("--dry-run")
        .help("Only list patches that would be uploaded, without actually uploading them.")
        .flag();

    argparse::ArgumentParser crack_command("crack", "Brute-force crack a RAR archive");
    crack_command.add_argument("rar_file")
        .help("The path to the .rar file to crack.");
    crack_command.add_argument("--threads")
        .help("Number of threads to use (default: 2)")
        .scan<'i', int>()
        .default_value(2);
    crack_command.add_argument("--json")
        .help("Output status as JSON instead of cracking")
        .flag();

    program.add_subparser(clean_command);
    program.add_subparser(run_command);
    program.add_subparser(build_command);
    program.add_subparser(download_command);
    program.add_subparser(upload_patch_command);
    program.add_subparser(crack_command);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [--muuk-path <path>] [other options]\n";
        return 1;
    }

    try {
        program.parse_args(argc, argv);
        std::string muuk_path = program.get<std::string>("--muuk-path");

        logger->info("[muuk] Using muuk configuration from: {}", muuk_path);

        MuukFiler muukFiler(muuk_path);
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
            }},
            {"upload-patch", [&]() {
                bool dry_run = upload_patch_command.get<bool>("--dry-run");
                logger->info("[muuk] Running upload-patch with dry-run: {}", dry_run);
                muuk.upload_patch(dry_run);
            }},
            {"crack", [&]() {
                const auto rar_file = crack_command.get<std::string>("rar_file");
                int threads = crack_command.get<int>("--threads");
                bool json_output = crack_command.get<bool>("--json");

                if (json_output) {
                    status_to_json(rar_file.c_str());
                }
                else {
                    crack(rar_file.c_str(), threads);
                }
            }},
        };

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
