#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <string>
#include <vector>

#include <argparse/argparse.hpp>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

#include "buildconfig.h"
#include "commands/add.hpp"
#include "commands/build.hpp"
#include "commands/clean.hpp"
#include "commands/init.hpp"
#include "commands/install.hpp"
#include "commands/remove.hpp"
#include "commands/run.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "version.h"

inline int check_and_report(const Result<void>& result) {
    if (!result) {
        if (!result.error().message.empty()) {
            muuk::logger::error(result);
        }
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {

    argparse::ArgumentParser program("muuk", VERSION);

    program.add_argument("--muuk-path")
        .help("Specify the path to muuk.toml")
        .default_value(std::string("muuk.toml"));

    argparse::ArgumentParser clean_command("clean", "Clean the project");
    clean_command.add_argument("clean_args")
        .remaining()
        .help("Build arguments passed to the build system")
        .default_value(std::vector<std::string> {});

    argparse::ArgumentParser run_command("run", "Run a custom script");
    run_command.add_argument("script")
        .help("The script to run, as defined in muuk.json");
    run_command.add_argument("extra_args")
        .remaining()
        .help("Extra arguments passed to the command")
        .default_value(std::vector<std::string> {});

    argparse::ArgumentParser build_command("build", "Build the project");
    build_command.add_argument("-t", "--target-build")
        .help("Specify a specific build target")
        .default_value(std::string(""))
        .nargs(1);
    build_command.add_argument("-c", "--compiler")
        .help("Specify a compiler to use (e.g., gcc, clang, cl)")
#ifdef _WIN32
        .default_value(std::string("cl")) // MSVC
#elif defined(__APPLE__)
        .default_value(std::string("clang")) // Apple Clang
#elif defined(__linux__)
        .default_value(std::string("gcc")) // GNU Linux
#endif
        .nargs(1);
    build_command.add_argument("-p", "--profile")
        .help("Specify a build profile (e.g., debug, release)")
        .default_value(std::string("")) // TODO: Eventually parse the default profile from the config file
        .nargs(1);
    build_command.add_argument("-j", "--jobs")
        .help("Number of jobs to run in parallel (0 means infinity)")
        .default_value(std::string("1"))
        .nargs(1);

    argparse::ArgumentParser download_command("install", "Install a package from github");

    argparse::ArgumentParser remove_command("remove", "Remove an installed package or submodule");
    remove_command.add_argument("package_name")
        .help("The name of the package to remove");

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
    program.add_subparser(init_command);
    program.add_subparser(add_command);

    if (argc < 2) {
        fmt::print("Usage: {} <command> [--muuk-path <path>] [other options]", std::string(argv[0]));
        return 1;
    }

    try {
        program.parse_args(argc, argv);
        std::string muuk_path = program.get<std::string>("--muuk-path");

        // ===================================================
        // Commands that don't require `muuk.toml`
        // ===================================================
        if (program.is_subcommand_used("init")) {
            return check_and_report(muuk::init_project());
        }

        if (program.is_subcommand_used("install")) {
            muuk::logger::info("Installing dependencies from muuk.toml...");
            return check_and_report(muuk::install("muuk.lock"));
        }

        if (program.is_subcommand_used("remove")) {
            const auto package_name = remove_command.get<std::string>("package_name");
            muuk::logger::info("Removing dependency: {}", package_name);
            return check_and_report(muuk::remove_package(
                package_name,
                "muuk.toml"));
        }

        if (program.is_subcommand_used("add")) {
            std::string dependency_name = add_command.get<std::string>("name");
            std::string version = add_command.get<std::string>("--version");
            // std::string revision = add_command.get<std::string>("--rev");
            // std::string tag = add_command.get<std::string>("--tag");
            // std::string branch = add_command.get<std::string>("--branch");
            std::string git_url = add_command.get<std::string>("--git");
            std::string muuk_path_dependency = ""; // add_command.get<std::string>("--path");
            std::string target_section = add_command.get<std::string>("--target");
            bool is_system = add_command.get<bool>("--sys");

            muuk::logger::info("Adding dependency: {}", dependency_name);

            auto result = muuk::add(
                muuk_path,
                dependency_name,
                version,
                git_url,
                muuk_path_dependency,
                // revision,
                // tag,
                // branch,
                is_system,
                target_section);

            if (!result) {
                muuk::logger::error(result);
                return 1;
            }

            return 0;
        }

        // ===================================================
        // Commands that require `muuk.toml`
        // ===================================================
        muuk::logger::info("[muuk] Using configuration from: {}", muuk_path);

        const auto parse_result = muuk::parse_muuk_file(
            muuk_path,
            false);
        if (!parse_result) {
            muuk::logger::error(parse_result);
            return 1;
        }

        auto muuk_config = parse_result.value().as_table();

        if (program.is_subcommand_used("clean")) {
            return check_and_report(muuk::clean(muuk_config));
        }

        if (program.is_subcommand_used("run")) {
            const auto script = run_command.present<std::string>("script");
            const auto extra_args = run_command.get<std::vector<std::string>>("extra_args");
            if (!script.has_value()) {
                muuk::logger::error("No script name provided for 'run'.");
                return 1;
            }
            return check_and_report(muuk::run_script(
                muuk_config,
                script.value(),
                extra_args));
        }

        if (program.is_subcommand_used("build")) {
            std::string target_build = build_command.get<std::string>("--target-build");
            std::string compiler = build_command.get<std::string>("--compiler");
            std::string profile = build_command.get<std::string>("--profile");
            std::string jobs = build_command.get<std::string>("--jobs");
            return check_and_report(muuk::build_cmd(
                target_build,
                compiler,
                profile,
                muuk_config,
                jobs));
        }
    } catch (const std::runtime_error& err) {
        muuk::logger::error(std::string(err.what()) + "\n");
        return 1;
    }

    return 0;
}