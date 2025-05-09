#include <filesystem>
#include <fstream>
#include <iostream>

#include <fmt/core.h>
#include <toml.hpp>

#include "buildconfig.h"
#include "logger.hpp"
#include "rustify.hpp"
#include "toml11/types.hpp"
#include "toml11/value.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {

    std::string prompt_with_default(const std::string& label, const std::string& default_value) {
        std::string input;
        std::cout << label << " (" << default_value << "): ";
        std::getline(std::cin, input);
        return input.empty() ? default_value : input;
    }

    Result<void> init_project() {
        muuk::logger::info("Initializing a new muuk.toml configuration...");

        std::string project_name;

        // clang-format off
        // Copied from `npm init`
        fmt::print(R"(This utility will walk you through creating a {} file.
It only covers the most common items, and tries to guess sensible defaults.
)", MUUK_TOML_FILE);
        //clang-format on

        std::cout << "project name: ";
        std::getline(std::cin, project_name);
        if (project_name.empty())
            return Err("Must specify project name.");

        const auto author = prompt_with_default("author", "");
        const auto version = prompt_with_default("version", "1.0.0");
        const auto license = prompt_with_default("license", "MIT");
        const auto entry_point = prompt_with_default("entry point", "src/main.cpp");
        const auto include_path = prompt_with_default("include path", "include/");

        fs::create_directories("src");
        fs::create_directories("include");

        // https://github.com/ToruNiina/toml11/issues/71#issuecomment-583098444
        using ordered_toml = toml::basic_value<toml::ordered_type_config>;

        ordered_toml root = {};

        ordered_toml package = {};
        package["name"] = project_name;
        package["author"] = author;
        package["version"] = version;
        package["license"] = license;

        root["package"] = package;

        ordered_toml library = {};
        library["include"] = toml::array { include_path };
        library["libs"] = toml::array {};
        library["sources"] = toml::array { "src/" + project_name + ".cpp" };

        ordered_toml project_build = {};
        project_build["cflags"] = toml::array {
            "/std:c++20",
            "/utf-8",
            "/EHsc",
            "/FS"
        };
        project_build["sources"] = toml::array { "src/" + entry_point };

        ordered_toml build = {};
        build[project_name] = project_build;
        root["build"] = build;

        // root["profile"] = toml::table {
        //     { "debug", toml::table { { "cflags", toml::array { "-g", "-O0", "-DDEBUG", "-Wall" } } } },
        //     { "release", toml::table { { "cflags", toml::array { "-O3", "-DNDEBUG", "-march=native" } } } },
        //     { "tests", toml::table { { "cflags", toml::array { "-g", "-O0", "-DTESTING", "-Wall" } } } }
        // };

        // root["platform"] = toml::table {
        //     { "windows", toml::table { { "cflags", toml::array { "/I." } } } },
        //     { "linux", toml::table { { "cflags", toml::array { "-pthread", "-rdynamic" } } } },
        //     { "macos", toml::table { { "cflags", toml::array { "-stdlib=libc++", "-mmacosx-version-min=10.13" } } } }
        // };

        muuk::logger::trace("Successfully created structured muuk.toml!");

        std::ifstream toml_file("muuk.toml");
        if (toml_file.is_open()) {
            std::cout << "\nCurrent muuk.toml content:\n";
            std::cout << toml_file.rdbuf();
            toml_file.close();
        }

        std::string confirmation;
        std::cout << "\nIs this OK? (yes): ";
        std::getline(std::cin, confirmation);
        if (!util::string_ops::to_lower(confirmation).starts_with("y")) {
            std::cout << "Initialization aborted.\n";
            return {};
        }

        // Write muuk.toml
        std::ofstream config_file("muuk.toml", std::ios::out | std::ios::trunc);
        if (!config_file.is_open())
            return Err("Failed to create muuk.toml.");

        config_file << toml::format(root);
        config_file.close();

        std::cout << "\nSuccessfully initialized muuk project!\n";

        // Create sample main file
        std::ofstream main_file("src/" + entry_point);
        if (main_file) {
            main_file << R"(#include <iostream>

int main() {
    std::cout << "Muuk was here." << std::endl;
    return 0;
})";
            main_file.close();
        }

        // Header
        std::ofstream header_file("include/" + project_name + ".hpp");
        if (header_file) {
            header_file << R"(#pragma once

void hello_muuk();)";
            header_file.close();
        }

        // Library source file
        std::ofstream lib_file("src/" + project_name + ".cpp");
        if (lib_file) {
            lib_file << "#include <iostream>\n"
                     << "#include \"include/" << project_name << ".h\"\n\n"
                     << "void hello_muuk() {\n"
                     << "    std::cout << \"This is a file in " << project_name << " library!\" << std::endl;\n"
                     << "}";
            lib_file.close();
        }

        muuk::logger::trace("Project structure initialized.");
        return {};
    }
} // namespace muuk