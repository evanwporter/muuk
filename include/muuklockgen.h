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

struct Dependency {
    std::string name;
    std::string version;
};

// { Dependency{ Versioning { Dependency Info } } }
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> DependencyMap;



class Component {
public:
    Component(const std::string& name,
        const std::string& version,
        const std::string& base_path,
        const std::string& package_type);

    void add_include_path(const std::string& path);

    void merge(const Component& child_pkg);

    std::string serialize() const;

    std::string name;
    std::string version;
    std::string base_path;
    std::string package_type; // "library" or "build"

    std::set<std::string> include;
    std::set<std::string> system_include;
    std::set<std::string> cflags;
    std::set<std::string> gflags;
    std::vector < std::pair<std::string, std::vector<std::string>>> sources;
    std::vector<std::string> modules;
    std::vector<std::string> libs;
    std::set<std::string> inherited_profiles;

    std::unordered_map<std::string, std::unordered_map<std::string, std::set<std::string>>> platform_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::set<std::string>>> compiler_;

    std::set<std::string> deps;
    std::vector<std::string> deps_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dependencies;

};

// struct Component {
//     std::string name;
//     std::string version;
//     std::string base_path;

//     DependencyMap global_dependencies;
//     std::unordered_map<std::string, Component> components;
// };

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);
    void generate_lockfile(const std::string& output_path);

private:
    std::string base_path_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<Component>>> resolved_packages_;
    std::unordered_map<std::string, std::shared_ptr<Component>> resolved_packages;
    std::unordered_map<std::string, std::shared_ptr<Component>> builds;


    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<MuukModuleParser> module_parser_;

    std::unordered_map<std::string, toml::table> dependencies_;

    std::vector<Dependency> deps_;

    DependencyMap global_deps_;
    std::vector<std::string> components_;

    std::unordered_set<std::string> visited;
    std::vector<std::string> resolved_order_;

    std::set<std::string> system_include_paths_;
    std::set<std::string> system_library_paths_;

    std::unordered_map<std::string, std::unordered_map<std::string, std::set<std::string>>> profiles_;

    void parse_section(const toml::table& section, Component& package);
    void search_and_parse_dependency(const std::string& package_name);

    // TODO: Use or Remove
    void process_modules(const std::vector<std::string>& module_paths, Component& package);

    std::optional<std::shared_ptr<Component>> find_package(const std::string& package_name);

    // TODO: Use or Remove
    void resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Component>> package);

    void merge_profiles(const std::string& base_profile, const std::string& inherited_profile);

    static Result<DependencyMap> parse_dependencies(const toml::table& dependencies_table);

    void parse_muuk_toml(const std::string& path, bool is_base = false);
    tl::expected<void, std::string> resolve_dependencies(const std::string& package_name, std::optional<std::string> search_path = std::nullopt);
};

#endif // MUUK_PARSER_H
