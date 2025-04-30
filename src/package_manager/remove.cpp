#include <filesystem>
#include <fstream>
#include <string>

#include <toml.hpp>

#include "buildconfig.h"
#include "commands/remove.hpp"
#include "lockgen/muuklockgen.hpp"
#include "muuk_parser.hpp"
#include "muukterminal.hpp"
#include "rustify.hpp"

namespace fs = std::filesystem;

namespace muuk {
    Result<void> remove_package(const std::string& package_name, const std::string& toml_path, const std::string& lockfile_path) {
        using namespace muuk::terminal;

        std::cout << style::CYAN << "Removing dependency: " << style::BOLD << package_name << style::RESET << "\n";

        try {
            auto parsed = muuk::parse_muuk_file(toml_path, false, true);
            if (!parsed)
                return Err(parsed);
            auto root = parsed.value();

            // TODO: Rather than copy and pasting this line, store it somewhere
            if (!root.contains("dependencies") || !root.at("dependencies").is_table()) {
                std::cout << style::YELLOW << "Dependency '" << package_name << "' not found. Nothing to do.\n"
                          << style::RESET;
                return {};
            }

            const auto& original_deps = root.at("dependencies").as_table();

            if (!original_deps.contains(package_name)) {
                std::cout << style::YELLOW << "Dependency '" << package_name << "' not found. Nothing to do.\n"
                          << style::RESET;
                return {};
            }

            std::cout << style::MAGENTA << "Found dependency '" << package_name << "'. Removing...\n"
                      << style::RESET;

            // Reconstruct the dependency table
            toml::table new_deps;
            for (const auto& [key, value] : original_deps) {
                if (key != package_name)
                    new_deps[key] = value;
            }
            root["dependencies"] = new_deps;

            // Remove local dependency folder
            const std::string dep_path = std::string(DEPENDENCY_FOLDER) + "/" + package_name;
            if (fs::exists(dep_path)) {
                std::cout << style::CYAN << "Deleting local folder: " << dep_path << style::RESET << "\n";
                fs::remove_all(dep_path);
            }

            auto lockgen = MuukLockGenerator::create(lockfile_path);
            if (!lockgen)
                return Err(lockgen.error());

            auto lockfile = lockgen->generate_lockfile(lockfile_path);
            if (!lockfile)
                return Err(lockfile.error());

            // Write the updated TOML
            std::ofstream file_out(toml_path, std::ios::trunc);
            if (!file_out.is_open())
                return Err("Failed to open TOML file for writing: {}", toml_path);

            file_out << toml::format(root);
            file_out.close();

            std::cout << style::GREEN << style::BOLD << "Successfully removed '" << package_name << "' from muuk.toml!" << style::RESET << "\n";
            return {};

        } catch (const std::exception& e) {
            return Err("Exception during removal: {}", std::string(e.what()));
        }
    }
}
