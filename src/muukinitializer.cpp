#include <filesystem>
#include <fstream>
#include <iostream>

#include <fmt/core.h>
#include <toml.hpp>

#include "logger.hpp"
#include "rustify.hpp"

namespace fs = std::filesystem;

namespace muuk {
    Result<void> init_project() {
        muuk::logger::info("Initializing a new muuk.toml configuration...");

        std::string project_name, author, version, license, include_path;

        std::cout << "Enter project name: ";
        std::getline(std::cin, project_name);
        if (project_name.empty()) {
            muuk::logger::error("Must specify project name.");
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

        toml::value root = toml::table {};

        root["package"] = toml::table {
            { "name", project_name },
            { "author", author },
            { "version", version },
            { "license", license }
        };

        root["scripts"] = toml::table {
            { "hello", "echo Hello, Muuk!" }
        };

        root["clean"] = toml::table {
            { "patterns", toml::array { "*.obj", "*.lib", "*.pdb", "*.o", "*.a", "*.so", "*.dll", "*.exe" } }
        };

        root["library"] = toml::table {
            { project_name, toml::table { { "include", toml::array { include_path } }, { "libs", toml::array {} }, { "sources", toml::array { "src/" + project_name + ".cpp" } }, { "dependencies", toml::table {} } } }
        };

        root["build"] = toml::table {
            { "bin", toml::table { { "dependencies", toml::table { { project_name, "v" + version } } }, { "gflags", toml::array { "/std:c++20", "/utf-8", "/EHsc", "/FS" } }, { "sources", toml::array { "src/main.cpp" } } } }
        };

        root["profile"] = toml::table {
            { "debug", toml::table { { "cflags", toml::array { "-g", "-O0", "-DDEBUG", "-Wall" } } } },
            { "release", toml::table { { "cflags", toml::array { "-O3", "-DNDEBUG", "-march=native" } } } },
            { "tests", toml::table { { "cflags", toml::array { "-g", "-O0", "-DTESTING", "-Wall" } } } }
        };

        root["platform"] = toml::table {
            { "windows", toml::table { { "cflags", toml::array { "/I." } } } },
            { "linux", toml::table { { "cflags", toml::array { "-pthread", "-rdynamic" } } } },
            { "macos", toml::table { { "cflags", toml::array { "-stdlib=libc++", "-mmacosx-version-min=10.13" } } } }
        };

        // Write muuk.toml
        std::ofstream config_file("muuk.toml", std::ios::out | std::ios::trunc);
        if (!config_file.is_open()) {
            muuk::logger::error("Failed to create muuk.toml.");
            return Err("");
        }

        config_file << toml::format(root);
        config_file.close();

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

        // Create sample main file
        std::ofstream main_file("src/main.cpp");
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