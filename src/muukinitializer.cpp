#include <filesystem>
#include <fstream>
#include <iostream>

#include <fmt/core.h>

#include "logger.h"
#include "muuk.h"
#include "muukfiler.h"
#include "rustify.hpp"

namespace fs = std::filesystem;

namespace muuk {

    Result<void> init_project() {
        muuk::logger::info("Initializing a new muuk.toml configuration...");

        std::string project_name, author, version, license, include_path;

        std::cout << "Enter project name: ";
        std::getline(std::cin, project_name);

        if (project_name.empty()) {
            muuk::logger::error("must specify project name");
            return Err("");
        }

        std::cout << "Enter author name: ";
        std::getline(std::cin, author);

        std::cout << "Enter project version (default: 0.1.0): ";
        std::getline(std::cin, version);
        if (version.empty())
            version = "0.1.0";

        std::cout << "Enter license (e.g., MIT, GPL, Apache, Unlicensed, default: MIT): ";
        std::getline(std::cin, license);
        if (license.empty())
            license = "MIT";

        std::cout << "Enter include path (default: include/): ";
        std::getline(std::cin, include_path);
        if (include_path.empty())
            include_path = "include/";

        fs::create_directories("src");
        fs::create_directories("include");

        std::ofstream config_file("muuk.toml", std::ios::out | std::ios::trunc);
        if (!config_file.is_open()) {
            muuk::logger::error("Failed to create muuk.toml.");
            return Err("");
        }

        config_file << "[package]\n";
        config_file << "name = '" << project_name << "'\n";
        config_file << "author = '" << author << "'\n";
        config_file << "version = '" << version << "'\n";
        config_file << "license = '" << license << "'\n\n";
        config_file.close();

        auto result = MuukFiler::create("muuk.toml");
        if (!result) {
            muuk::logger::error("Failed to create MuukFiler: {}", result.error());
            return Err("");
        }
        MuukFiler filer = result.value();

        toml::table package_section {
            { "name", project_name },
            { "author", author },
            { "version", version },
            { "license", license }
        };
        filer.modify_section("package", package_section);

        toml::table scripts_section {
            { "hello", "echo Hello, Muuk!" }
        };
        filer.modify_section("scripts", scripts_section);

        toml::table clean_section {
            { "patterns", toml::array { "*.obj", "*.lib", "*.pdb", "*.o", "*.a", "*.so", "*.dll", "*.exe" } }
        };
        filer.modify_section("clean", clean_section);

        toml::table library_section {
            { "include", toml::array { include_path } },
            { "libs", toml::array {} },
            { "sources", toml::array { "src/" + project_name + ".cpp" } },
            { "dependencies", toml::table {} }
        };
        filer.modify_section("library." + project_name, library_section);

        toml::table build_section {
            { "dependencies", toml::table { { project_name, "v" + version } } },
            // TODO: Remove
            { "gflags", toml::array { "/std:c++20", "/utf-8", "/EHsc", "/FS" } },
            { "sources", toml::array { "src/main.cpp" } }
        };
        filer.modify_section("build.bin", build_section);

        toml::table profiles_section;
        profiles_section.insert_or_assign("debug", toml::table { { "cflags", toml::array { "-g", "-O0", "-DDEBUG", "-Wall" } } });
        profiles_section.insert_or_assign("release", toml::table { { "cflags", toml::array { "-O3", "-DNDEBUG", "-march=native" } } });
        profiles_section.insert_or_assign("tests", toml::table { { "cflags", toml::array { "-g", "-O0", "-DTESTING", "-Wall" } } });
        filer.modify_section("profile", profiles_section);

        toml::table platform_section;
        platform_section.insert_or_assign("windows", toml::table { { "cflags", toml::array { "/I." } } });
        platform_section.insert_or_assign("linux", toml::table { { "cflags", toml::array { "-pthread", "-rdynamic" } } });
        platform_section.insert_or_assign("macos", toml::table { { "cflags", toml::array { "-stdlib=libc++", "-mmacosx-version-min=10.13" } } });
        filer.modify_section("platform", platform_section);

        filer.write_to_file();

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
        if (confirmation != "yes") {
            std::cout << "Initialization aborted.\n";
            return {};
        }

        std::cout << "\nSuccessfully initialized muuk project!\n";

        std::ofstream main_file("src/main.cpp");
        if (main_file) {
            main_file << R"(#include <iostream>

int main() {
    std::cout << "Muuk was here." << std::endl;
    return 0;
})";
            main_file.close();
        }

        std::ofstream header_file("include/" + project_name + ".h");
        if (header_file) {
            header_file << R"(#pragma once

void hello_muuk();)";
            header_file.close();
        }

        std::ofstream lib_file("src/" + project_name + ".cpp");
        if (lib_file) {
            lib_file << R"(#include <iostream>
#include "include/" + project_name + R"(.h"

void hello_muuk() {
    std::cout << "This is a file in " << )"
                     << project_name << R"( << " library!" << std::endl;
})";
            lib_file.close();
        }

        muuk::logger::trace("Project structure initialized.");
        return {};
    }
}
