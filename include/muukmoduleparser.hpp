#ifndef MUUK_MODULE_PARSER_HPP
#define MUUK_MODULE_PARSER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include "../include/logger.h"

namespace fs = std::filesystem;

const std::regex EXPORT_MODULE_PATTERN(R"(^\s*export\s+module\s+([\w.]+);)");
const std::regex IMPORT_PATTERN(R"(^\s*(?:export\s+)?import\s+([\w.]+);)");

const std::regex SINGLE_LINE_COMMENT(R"(//.*)");
const std::regex MULTI_LINE_COMMENT(R"(/\*.*?\*/)");

struct Module {
    std::string exported_module;
    std::vector<std::string> imported_modules;
    std::string file_path;
};

class MuukModuleParser {
private:
    std::string toml_path;
    std::unordered_map<std::string, Module> modules_dict;
    std::vector<std::string> resolved_modules;
    std::unordered_set<std::string> visiting;
    toml::table config;
    std::shared_ptr<spdlog::logger> logger_;

public:
    explicit MuukModuleParser(const std::string& path) : toml_path(path) {
        muuk::logger::trace("Initializing MuukModuleParser with path: {}", path);
        loadToml();
    }

    void loadToml() {
        try {
            muuk::logger::trace("Loading TOML configuration from: {}muuk.toml", toml_path);
            config = toml::parse_file(toml_path + "/muuk.toml");
            muuk::logger::trace("TOML configuration loaded successfully.");
        }
        catch (const std::exception& e) {
            muuk::logger::error("Error loading TOML file: {}", e.what());
        }
    }

    std::vector<std::string> getModulePaths() {
        std::vector<std::string> module_paths;
        auto library_config = config["library"]["muuk"]["modules"];

        if (!library_config.is_array()) {
            muuk::logger::error("Invalid or missing 'modules' list in TOML.");
            return module_paths;
        }

        muuk::logger::info("Retrieving module paths from TOML configuration...");
        for (const auto& pattern : *library_config.as_array()) {
            if (pattern.is_string()) {
                std::string pattern_str = pattern.as_string()->get();
                muuk::logger::info("Searching for modules matching pattern: {}", pattern_str);
                for (const auto& entry : fs::recursive_directory_iterator(toml_path)) {
                    if (entry.path().string().find(pattern_str) != std::string::npos) {
                        module_paths.push_back(entry.path().string());
                        muuk::logger::info("Found module file: {}", entry.path().string());
                    }
                }
            }
        }
        return module_paths;
    }

    std::string removeComments(const std::string& content) {
        muuk::logger::info("Removing comments from source file...");
        std::string cleaned = std::regex_replace(content, MULTI_LINE_COMMENT, "");
        cleaned = std::regex_replace(cleaned, SINGLE_LINE_COMMENT, "");
        return cleaned;
    }

    Module parseCppModule(const std::string& file_path) {
        muuk::logger::info("Parsing C++ module file: {}", file_path);

        // TODO: it gives an error if the module is in the same dir as the one the program is run from
        std::ifstream file(file_path);
        if (!file) {
            muuk::logger::error("Error reading file: {}", file_path);
            return { "", {}, file_path };
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = removeComments(buffer.str());

        std::smatch export_match;
        std::string module_name;
        if (std::regex_search(content, export_match, EXPORT_MODULE_PATTERN)) {
            module_name = export_match[1].str();
            muuk::logger::info("Detected exported module: {}", module_name);
        }

        std::vector<std::string> imports;
        auto begin = std::sregex_iterator(content.begin(), content.end(), IMPORT_PATTERN);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            imports.push_back((*it)[1].str());
            muuk::logger::info("Detected import: {}", (*it)[1].str());
        }

        return { module_name, imports, file_path };
    }

    void parseAllModules(const std::vector<std::string>& module_paths) {
        muuk::logger::info("Parsing specified module files...");

        for (const auto& file_path : module_paths) {
            Module mod = parseCppModule(file_path);
            if (!mod.exported_module.empty()) {
                modules_dict[mod.exported_module] = mod;
                muuk::logger::info("Registered module: {} -> {}", mod.exported_module, mod.file_path);
            }
        }

        muuk::logger::info("Finished parsing modules.");
    }

    void resolveModuleOrder(const std::string& module_name) {
        if (std::find(resolved_modules.begin(), resolved_modules.end(), module_name) != resolved_modules.end()) {
            muuk::logger::info("Module '{}' already resolved, skipping...", module_name);
            return;
        }

        if (visiting.find(module_name) != visiting.end()) {
            muuk::logger::error("Circular dependency detected: {}", module_name);
            throw std::runtime_error("Circular dependency detected: " + module_name);
        }

        if (modules_dict.find(module_name) == modules_dict.end()) {
            muuk::logger::warn("Warning: {} is imported but not found in modules!", module_name);
            return;
        }

        muuk::logger::info("Resolving dependencies for: {}", module_name);
        visiting.insert(module_name);

        for (const auto& dep : modules_dict[module_name].imported_modules) {
            resolveModuleOrder(dep);
        }

        visiting.erase(module_name);
        resolved_modules.push_back(module_name);
        muuk::logger::info("Resolved module: {}", module_name);
    }

    std::vector<std::string> resolveAllModules() {
        muuk::logger::info("Resolving dependency order for all modules...");

        std::vector<std::string> resolved_paths;

        for (const auto& [module_name, module] : modules_dict) {
            resolveModuleOrder(module_name);
        }

        muuk::logger::info("Compilation order resolved. Correct order:");
        for (const auto& mod_name : resolved_modules) {
            const auto& mod = modules_dict[mod_name];
            resolved_paths.push_back(mod.file_path);
            muuk::logger::info("    {} -> {}", mod_name, mod.file_path);
        }

        return resolved_paths;
    }

    const std::vector<std::string>& getResolvedCompilationOrder() {
        return resolved_modules;
    }
};

#endif // MUUK_MODULE_PARSER_HPP
