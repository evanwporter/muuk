#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include "../include/muukfiler.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <unordered_set>

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
    std::set<std::string> libflags;
    std::set<std::string> lflags;
    std::vector<std::string> sources;
    std::map<std::string, std::string> dependencies;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);

    void parse_muuk_toml(const std::string& path, bool is_base = false);
    void resolve_dependencies(const std::string& package_name);
    void generate_lockfile(const std::string& output_path);

    const std::map<std::string, std::map<std::string, std::shared_ptr<Package>>>& get_resolved_packages() const {
        return resolved_packages_;
    }

private:
    std::string base_path_;
    std::map<std::string, std::map<std::string, std::shared_ptr<Package>>> resolved_packages_;
    std::shared_ptr<spdlog::logger> logger_;

    std::unordered_set<std::string> visited;

    void parse_section(const toml::table& section, Package& package);
    void search_and_parse_dependency(const std::string& package_name);

    std::optional<std::shared_ptr<Package>> find_package(const std::string& package_name);

};

#endif // MUUK_PARSER_H
