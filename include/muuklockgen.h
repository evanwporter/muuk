#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include "../include/muukfiler.h"
#include "../include/muukmoduleparser.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <set>
#include <vector>
#include <memory>
#include <unordered_set>
#include <optional>

namespace fs = std::filesystem;

class Package {
public:
    Package(const std::string& name,
        const std::string& version,
        const std::string& base_path,
        const std::string& package_type);

    void merge(const Package& child_pkg);
    toml::table serialize() const;

    std::string name;
    std::string version;
    std::string base_path;
    std::string package_type; // "library" or "build"

    std::set<std::string> include;
    std::set<std::string> system_include;
    std::set<std::string> cflags;
    std::set<std::string> gflags;
    std::vector<std::string> sources;
    std::vector<std::string> modules;
    std::vector<std::string> libs;

    std::set<std::string> deps;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dependencies;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);
    void generate_lockfile(const std::string& output_path, bool is_release = false);

private:
    std::string base_path_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<Package>>> resolved_packages_;
    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<MuukModuleParser> module_parser_;

    std::unordered_map<std::string, toml::table> dependencies_;

    std::unordered_set<std::string> visited;
    std::vector<std::string> resolved_order_;

    std::set<std::string> system_include_paths_;
    std::set<std::string> system_library_paths_;

    std::unordered_map<std::string, std::set<std::string>> platform_cflags_;

    void parse_section(const toml::table& section, Package& package);
    void search_and_parse_dependency(const std::string& package_name);

    void process_modules(const std::vector<std::string>& module_paths, Package& package);

    std::optional<std::shared_ptr<Package>> find_package(const std::string& package_name);

    void resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Package>> package);

    void parse_muuk_toml(const std::string& path, bool is_base = false);
    void resolve_dependencies(const std::string& package_name, std::optional<std::string> search_path = std::nullopt);
};

#endif // MUUK_PARSER_H
