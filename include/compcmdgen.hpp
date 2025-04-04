#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

#include "buildconfig.h"
#include "logger.h"
#include "muukfiler.h"
#include "util.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

class CompileCommandsGenerator {
public:
    explicit CompileCommandsGenerator(const std::string& lockfile_path) :
        lockfile_path_(lockfile_path) {
        muuk::logger::info("[CompileCommandsGenerator] Initializing with lockfile: '{}'", lockfile_path_);
    }

    void generate_compile_commands() {
        muuk_filer_ = std::make_unique<MuukFiler>(lockfile_path_);
        config_ = muuk_filer_->get_config();

        if (config_.empty()) {
            muuk::logger::error("[CompileCommandsGenerator] Error: No TOML data loaded.");
            return;
        }

        muuk::logger::info("[CompileCommandsGenerator] Generating compile_commands.json...");
        json compile_commands = json::array();

        for (const auto& [pkg_type, packages] : config_) {
            if (pkg_type != "library" && pkg_type != "build")
                continue;

            auto packages_table = packages.as_table();

            for (const auto& [pkg_name, pkg_info] : packages_table) {
                process_package(pkg_name, pkg_info.as_table(), compile_commands);
            }
        }

        write_compile_commands(compile_commands);
    }

private:
    std::string lockfile_path_;
    std::unique_ptr<MuukFiler> muuk_filer_;
    toml::table config_;
    std::shared_ptr<spdlog::logger> logger_;

    void process_package(const std::string& package_name, const toml::table& package_data, json& compile_commands) {
        muuk::logger::info("[CompileCommandsGenerator] Processing package: {}", package_name);

        std::vector<std::string> includes, cflags, sources;
        std::string base_path;

        if (package_data.contains("base_path")) {
            base_path = package_data.at("base_path").as_string();
        }

        if (package_data.contains("include")) {
            for (const auto& inc : package_data.at("include").as_array()) {
                includes.push_back(util::to_linux_path((fs::path(base_path) / inc.as_string()).string()));
            }
        }

        if (package_data.contains("cflags")) {
            for (const auto& flag : package_data.at("cflags").as_array()) {
                cflags.push_back(*flag.as_string());
            }
        }

        if (package_data.contains("sources")) {
            for (const auto& src : package_data.at("sources").as_array()) {
                sources.push_back(util::to_linux_path((fs::path(base_path) / src.as_string()).string()));
            }
        }

        std::string compiler = COMPILER; // Defined in buildconfig.h
        std::string include_flags = generate_include_flags(includes);
        std::string cflags_str = util::normalize_flags(cflags);

        for (const auto& src : sources) {
            json entry;
            entry["directory"] = fs::current_path().string();
            entry["command"] = compiler + " " + cflags_str + " " + include_flags + " -c " + src + " -o " + fs::path(src).replace_extension(".o").string();
            entry["file"] = src;
            compile_commands.push_back(entry);

            muuk::logger::info("[CompileCommandsGenerator] Added compile command for {}", src);
        }
    }

    std::string generate_include_flags(const std::vector<std::string>& includes) {
        std::string flags;
        for (const auto& inc : includes) {
#ifdef _WIN32
            flags += "/I" + inc + " ";
#else
            flags += "-I" + inc + " ";
#endif
        }
        return flags;
    }

    void write_compile_commands(const json& compile_commands) {
        std::ofstream out("compile_commands.json");
        if (!out) {
            muuk::logger::error("[CompileCommandsGenerator] Failed to create compile_commands.json");
            throw std::runtime_error("Failed to create compile_commands.json.");
        }

        out << compile_commands.dump(4);
        out.close();
        muuk::logger::info("[CompileCommandsGenerator] compile_commands.json generated successfully!");
    }
};
