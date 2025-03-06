#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include "../include/muukfiler.h"
#include "../include/muukmoduleparser.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <unordered_set>
#include <optional>

namespace fs = std::filesystem;

struct Dependency {
    std::string name;
    std::string version;
};

enum class LinkType {
    STATIC,
    SHARED
};

enum class PackageType {
    LIBRARY,
    BUILD
};

struct BaseConfig {
    std::unordered_set<std::string> cflags;
    std::unordered_set<std::string> libflags;
    std::unordered_set<std::string> lflags;

    std::unordered_set<std::string> include;
    std::unordered_set<std::string> libs;
    std::unordered_set<std::string> defines;

    std::vector<std::pair<std::string, std::unordered_set<std::string>>> sources;
};

typedef BaseConfig LinkArgs;
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> Profiles;

class Package {
public:
    std::string name;
    std::string version;
    std::string base_path;
    std::string hash;

    std::unordered_set<std::string> cflags;
    std::unordered_set<std::string> libflags;
    std::unordered_set<std::string> lflags;
    PackageType package_type; // "library" or "build"


    std::unordered_set<std::string> include;
    std::unordered_set<std::string> libs;
    std::unordered_set<std::string> defines;

    std::vector<std::pair<std::string, std::unordered_set<std::string>>> sources;

    LinkType link_type = LinkType::STATIC;
    LinkArgs static_link_args;
    LinkArgs shared_link_args;

    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> platform_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> compiler_;
};

// { Dependency { Versioning { Dependency Info } } }
typedef DependencyVersionMap<std::unordered_map<std::string, std::string>> DependencyInfoMap;

class Component {
public:
    Component(
        const std::string& name,
        const std::string& version,
        const std::string& base_path,
        const std::string& package_type
    );

    void add_include_path(const std::string& path);

    void merge(const Component& child_pkg);

    std::string serialize() const;

    std::string name;
    std::string version;
    std::string base_path;
    std::string package_type; // "library" or "build"

    std::unordered_set<std::string> include;
    std::unordered_set<std::string> system_include;
    std::unordered_set<std::string> cflags;
    std::unordered_set<std::string> gflags;
    std::vector<std::pair<std::string, std::vector<std::string>>> sources;
    std::vector<std::string> modules;
    std::vector<std::string> libs;
    std::unordered_set<std::string> inherited_profiles;

    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> platform_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> compiler_;

    // Remove this stuff
    std::unordered_set<std::string> deps;
    std::vector<std::string> deps_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dependencies;

    // TODO store the package and add serialize as depedency method
    DependencyInfoMap dependencies_;

};

// { Dependency { Versioning { Package } } }
typedef DependencyVersionMap<std::shared_ptr<Component>> DependencyMap;

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);
    void generate_lockfile(const std::string& output_path);

private:
    std::string base_path_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<Component>>> resolved_packages_;
    DependencyMap resolved_packages;

    // std::unordered_map<std::string, std::shared_ptr<Component>> resolved_packages;
    std::unordered_map<std::string, std::shared_ptr<Component>> builds;

    std::shared_ptr<Component> base_package_;

    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<MuukModuleParser> module_parser_;

    DependencyVersionMap<toml::table> dependencies_;

    std::vector<Dependency> deps_;

    DependencyMap global_deps_;
    std::vector<std::string> components_;

    std::unordered_set<std::string> visited;

    // <Package, Version> vector array
    std::vector<std::pair<std::string, std::string>> resolved_order_;

    std::unordered_set<std::string> system_include_paths_;
    std::unordered_set<std::string> system_library_paths_;

    Profiles profiles_;

    static Result<void> parse_library(const toml::table& data, std::shared_ptr<Component> package);
    static Result<void> parse_platform(const toml::table& data, std::shared_ptr<Component> package);
    static Result<void> parse_compiler(const toml::table& data, std::shared_ptr<Component> package);
    static Result<void> parse_dependencies(const toml::table& data, std::shared_ptr<Component> package);
    Result<void> parse_profile(const toml::table& data);
    Result<void> parse_builds(
        const toml::table& data,
        const std::string& package_version,
        const std::string& path
    );

    void search_and_parse_dependency(const std::string& package_name, const std::string& version);

    // TODO: Use or Remove
    void process_modules(const std::vector<std::string>& module_paths, Component& package);

    std::optional<std::shared_ptr<Component>> find_package(const std::string& package_name, std::optional<std::string> version = std::nullopt);

    // TODO: Use or Remove
    void resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Component>> package);

    void merge_profiles(const std::string& base_profile, const std::string& inherited_profile);

    void parse_muuk_toml(const std::string& path, bool is_base = false);

    tl::expected<void, std::string> resolve_dependencies(const std::string& package_name, std::optional<std::string> version = std::nullopt, std::optional<std::string> search_path = std::nullopt);
};

#endif // MUUK_PARSER_H
