#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "buildmanager.h"
#include "compiler.hpp"
#include "flags.hpp"
#include "logger.h"
#include "muuk.h"
#include "util.h"

namespace muuk {

    // Generates the compilation database for clang-scan-deps
    nlohmann::json generate_compilation_database(BuildManager& build_manager, const std::string& build_dir) {
        nlohmann::json compdb = nlohmann::json::array();

        auto& compilation_targets = build_manager.get_compilation_targets();

        for (const auto& target : compilation_targets) {
            std::string command = "clang++ -x c++-module --std=c++23";

            // Append compilation flags
            for (const auto& flag : target.flags) {
                std::string normalized = muuk::normalize_flag(flag, muuk::Compiler::Clang);

                // Make -I paths absolute
                if (normalized.starts_with("-I")) {
                    std::string path = normalized.substr(2); // Strip -I
                    if (!std::filesystem::path(path).is_absolute()) {
                        std::filesystem::path abs_path = std::filesystem::absolute(build_dir + "/" + path);
                        normalized = "-I" + abs_path.string();
                    }
                }

                command += " " + normalized;
            }

            // TODO: Remove more elegantly
            std::string sanitized_input = target.input;
            sanitized_input.erase(
                std::remove(
                    sanitized_input.begin(),
                    sanitized_input.end(),
                    '$'),
                sanitized_input.end());

            // Append input file
            command += " " + sanitized_input;

            // Append output file
            command += " -o " + target.output;

            // Create a JSON object for this compilation command
            nlohmann::json entry;
            entry["directory"] = build_dir; // Use current directory
            entry["command"] = command;
            entry["file"] = sanitized_input;
            entry["output"] = target.output;

            // Add entry to the compilation database
            compdb.push_back(entry);
        }

        return compdb;
    }

    // Runs clang-scan-deps and retrieves dependency information
    nlohmann::json parse_dependency_db(const std::string& dependency_db) {
        auto out = util::command_line::execute_command_get_out(
            fmt::format("clang-scan-deps -format=p1689 -compilation-database {}", dependency_db));

        nlohmann::json dependencies = nlohmann::json::parse(out);

        if (!dependencies.contains("rules") || !dependencies["rules"].is_array()) {
            muuk::logger::error("Error: No valid 'rules' array found in dependency database.");
            return {};
        }

        return dependencies;
    }

    // Resolves provided modules and maps logical names to compilation targets
    void resolve_provided_modules(
        const nlohmann::json& dependencies,
        std::unordered_map<std::string, CompilationTarget*>& target_map) {

        for (const auto& rule : dependencies["rules"]) {
            if (!rule.contains("primary-output") || !rule["primary-output"].is_string()) {
                continue; // Skip invalid rules
            }

            std::string primary_output = rule["primary-output"];
            auto primary_target_it = target_map.find(primary_output);
            if (primary_target_it == target_map.end())
                continue;

            CompilationTarget* primary_target = primary_target_it->second;

            if (rule.contains("provides") && rule["provides"].is_array()) {
                for (const auto& provide : rule["provides"]) {
                    if (provide.contains("logical-name") && provide["logical-name"].is_string()) {
                        std::string logical_name = provide["logical-name"];
                        primary_target->logical_name = logical_name;
                        muuk::logger::info("Associated module '{}' with target '{}'", logical_name, primary_output);
                    }
                }
            }
        }
    }

    // Resolves required modules and links dependencies
    void resolve_required_modules(
        const nlohmann::json& dependencies,
        BuildManager& build_manager,
        std::unordered_map<std::string, CompilationTarget*>& target_map) {

        for (const auto& rule : dependencies["rules"]) {
            if (!rule.contains("primary-output") || !rule["primary-output"].is_string())
                continue;

            std::string primary_output = rule["primary-output"];
            auto primary_target_it = target_map.find(primary_output);
            if (primary_target_it == target_map.end())
                continue;

            CompilationTarget* primary_target = primary_target_it->second;

            if (rule.contains("requires") && rule["requires"].is_array()) {
                for (const auto& require : rule["requires"]) {
                    if (!require.contains("source-path") || !require["source-path"].is_string())
                        continue;

                    std::string required_source = require["source-path"];
                    CompilationTarget* required_target = build_manager.find_compilation_target("input", required_source);
                    if (required_target) {
                        primary_target->dependencies.push_back(required_target);
                        muuk::logger::info("Added dependency: Target '{}' requires '{}'", primary_output, required_source);
                    } else {
                        muuk::logger::warn("Could not find compilation target for required module '{}'", required_source);
                    }
                }
            }
        }
    }

    // Orchestrates module resolution
    void resolve_modules(BuildManager& build_manager, const std::string& build_dir) {
        std::string dependency_db = build_dir + "/dependency-db.json";

        auto compdb = generate_compilation_database(build_manager, build_dir);

        // Write to output file (dependency-db.json)
        std::ofstream out_file(dependency_db);
        if (!out_file) {
            muuk::logger::error("Could not open output file {} for writing!", dependency_db);
            return;
        }
        out_file << compdb.dump(4); // Pretty-print JSON with 4-space indentation
        out_file.close();

        muuk::logger::info("Compilation database written to {}", dependency_db);

        nlohmann::json dependencies = parse_dependency_db(dependency_db);
        if (dependencies.empty())
            return;

        std::unordered_map<std::string, CompilationTarget*> target_map;
        auto& compilation_targets = build_manager.get_compilation_targets();
        for (auto& target : compilation_targets) {
            target_map[target.output] = &target;
        }

        resolve_provided_modules(dependencies, target_map);
        resolve_required_modules(dependencies, build_manager, target_map);
    }

} // namespace muuk
