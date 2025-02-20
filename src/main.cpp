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
#include <cracklib.hpp>

#include "muuker.hpp"
#include "muukfiler.h"
#include "logger.h"
#include "util.h"
#include "muuk.h"

#include "package_manager.h"

namespace fs = std::filesystem;
// using namespace Muuk;

#ifdef DEBUG
void start_repl(std::unordered_map<std::string, std::function<void()>>& command_map) {
    std::cout << "\n=== DEBUG REPL MODE ===\n";
    std::cout << "Enter commands manually. Type 'exit' or press Ctrl+C to quit.\n";
    std::cout << "Available commands: ";

    // Print all available commands at the start
    for (const auto& [cmd, _] : command_map) {
        std::cout << cmd << " ";
    }
    std::cout << "\n\n";


    while (true) {
        std::cout << ">> ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "exit") {
            std::cout << "Exiting REPL mode...\n";
            break;
        }

        auto it = command_map.find(command);
        if (it != command_map.end()) {
            it->second();
        }
        else {
            std::cout << "Unknown command: " << command << "\n";
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    auto logger_ = logger::get_logger("main_logger");

    argparse::ArgumentParser program("muuk");

    program.add_argument("--muuk-path")
        .help("Specify the path to muuk.toml")
        .default_value(std::string("muuk.toml"));

#ifdef DEBUG
    program.add_argument("--repl")
        .help("Start REPL mode for debugging")
        .flag();
#endif

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
    build_command.add_argument("-t", "--target-build")
        .help("Specify a specific build target")
        .default_value(std::string(""))
        .nargs(1);
    build_command.add_argument("-c", "--compiler")
        .help("Specify a compiler to use (e.g., g++, clang++, cl)")
        .default_value(std::string(""))
        .nargs(1);
    build_command.add_argument("-p", "--profile")
        .help("Specify a build profile (e.g., debug, release)")
        .default_value(std::string(""))
        .nargs(1);

    argparse::ArgumentParser download_command("install", "Install a package from github");
    // download_command.add_argument("url")
    //     .help("The author and zip file of the github repo to install");
    // download_command.add_argument("--version")
    //     .help("The version to download (default: latest)")
    //     .default_value(std::string("latest"));
    // download_command.add_argument("--submodule")
    //     .help("Install the package as a Git submodule instead of downloading a release.")
    //     .flag();

    argparse::ArgumentParser remove_command("remove", "Remove an installed package or submodule");
    remove_command.add_argument("package_name")
        .help("The name of the package to remove");

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

    argparse::ArgumentParser init_command("init", "Initialize a new muuk.toml configuration file");

    argparse::ArgumentParser add_command("add", "Add a dependency to muuk.toml");
    add_command.add_argument("name")
        .help("The name of the dependency.");
    add_command.add_argument("--sys")
        .help("Marks the package as a system dependency (requires --version).")
        .flag();
    add_command.add_argument("--version")
        .help("Specify dependency version (Required for --sys).")
        .default_value(std::string(""));
    add_command.add_argument("--rev")
        .help("Specify revision for Git dependencies.")
        .default_value(std::string(""));
    add_command.add_argument("--tag")
        .help("Specify tag for Git dependencies.")
        .default_value(std::string(""));
    add_command.add_argument("--branch")
        .help("Specify branch for Git dependencies.")
        .default_value(std::string(""));
    add_command.add_argument("--git")
        .help("The Git repository URL (optional).")
        .default_value(std::string(""));
    add_command.add_argument("--muuk-path")
        .help("The path to the dependency's muuk.toml file (default: deps/<name>/muuk.toml).")
        .default_value(std::string(""));
    add_command.add_argument("-t", "--target")
        .help("Specify a target section to add the dependency (e.g., build.test, library.muuk).")
        .default_value(std::string(""));


    program.add_subparser(clean_command);
    program.add_subparser(run_command);
    program.add_subparser(build_command);
    program.add_subparser(download_command);
    program.add_subparser(remove_command);
    program.add_subparser(upload_patch_command);
    program.add_subparser(crack_command);
    program.add_subparser(init_command);
    program.add_subparser(add_command);


    if (argc < 2) {
        logger_->error("Usage: " + std::string(argv[0]) + " <command> [--muuk-path <path>] [other options]\n");
        return 1;
    }

    try {
        program.parse_args(argc, argv);
        std::string muuk_path = program.get<std::string>("--muuk-path");

        // Command execution logic
        if (program.is_subcommand_used("init")) {
            muuk::init_project();
            return 0;
        }

        if (program.is_subcommand_used("install")) {
            logger_->info("Installing dependencies from muuk.toml...");
            muuk::package_manager::install("muuk.lock.toml");
            return 0;
        }

        if (program.is_subcommand_used("remove")) {
            const auto package_name = remove_command.get<std::string>("package_name");
            logger_->info("Removing dependency: {}", package_name);
            muuk::package_manager::remove_package(package_name, "muuk.toml", "muuk.lock.toml");
            return 0;
        }

        // TODO: Do something with
        // if (program.is_subcommand_used("upload-patch")) {
        //     bool dry_run = upload_patch_command.get<bool>("--dry-run");
        //     logger_->info("[muuk] Running upload-patch with dry-run: {}", dry_run);
        //     muuk::upload_patch(dry_run);
        //     return 0;
        // }

        if (program.is_subcommand_used("crack")) {
            const auto rar_file = crack_command.get<std::string>("rar_file");
            int threads = crack_command.get<int>("--threads");
            bool json_output = crack_command.get<bool>("--json");
            crack(rar_file.c_str(), threads);
            return 0;
        }

        if (program.is_subcommand_used("add")) {
            std::string dependency_name = add_command.get<std::string>("name");
            std::string version = add_command.get<std::string>("--version");
            std::string revision = add_command.get<std::string>("--rev");
            std::string tag = add_command.get<std::string>("--tag");
            std::string branch = add_command.get<std::string>("--branch");
            std::string git_url = add_command.get<std::string>("--git");
            std::string muuk_path_dependency = add_command.get<std::string>("--muuk-path");
            std::string target_section = add_command.get<std::string>("--target");
            bool is_system = add_command.get<bool>("--sys");

            logger_->info("Adding dependency: {}", dependency_name);

            muuk::package_manager::add_dependency(
                muuk_path,
                dependency_name,
                version,
                git_url,
                muuk_path_dependency,
                revision,
                tag,
                branch,
                is_system,
                target_section
            );

            return 0;
        }

        // Commands that require `muuk.toml`
        logger_->info("[muuk] Using configuration from: {}", muuk_path);
        MuukFiler muukFiler(muuk_path);
        Muuker muuk(muukFiler);
        MuukBuilder muukBuilder(muukFiler);

        if (program.is_subcommand_used("clean")) {
            muuk.clean();
            return 0;
        }

        if (program.is_subcommand_used("run")) {
            const auto script = run_command.present<std::string>("script");
            const auto extra_args = run_command.get<std::vector<std::string>>("extra_args");
            if (!script.has_value()) {
                logger_->error("Error: No script name provided for 'run'.\n");
                return 1;
            }
            muuk.run_script(script.value(), extra_args);
            return 0;
        }

        if (program.is_subcommand_used("build")) {
            std::string target_build = build_command.get<std::string>("--target-build");
            std::string compiler = build_command.get<std::string>("--compiler");
            std::string profile = build_command.get<std::string>("--profile");
            muukBuilder.build(target_build, compiler, profile);

            return 0;
        }

#ifdef DEBUG
        // if (program.get<bool>("--repl")) {
        //     start_repl(command_map);
        //     return 0;
        // }
#endif
    }
    catch (const std::runtime_error& err) {
        logger_->error(std::string(err.what()) + "\n");
        return 1;
    }

    return 0;
}