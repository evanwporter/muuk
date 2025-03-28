#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

#include "base_config.hpp"
#include "compiler.hpp"
#include "muuk.h"
#include "rustify.hpp"

// { Dependency { Versioning { Dependency Info } } }
typedef DependencyVersionMap<Dependency> DependencyInfoMap;

// { Dependency { Versioning { Package } } }
typedef DependencyVersionMap<std::shared_ptr<Package>> DependencyMap;

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);
    static Result<MuukLockGenerator> create(const std::string& base_path);

    Result<void> load();

    Result<void> generate_lockfile(const std::string& output_path);
    Result<void> generate_cache(const std::string& output_path);

private:
    muuk::Edition edition_;

    std::string base_path_;

    DependencyMap resolved_packages;
    std::unordered_map<std::string, std::shared_ptr<Package>> builds;
    std::unordered_map<std::string, std::shared_ptr<Build>> builds_;

    std::shared_ptr<Package> base_package_;

    // Meant to be a general reference for packages to refer to
    DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_;

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visited_builds;

    // <Package, Version> vector array
    std::vector<std::pair<std::string, std::string>>
        resolved_order_;

    std::unordered_set<std::string> system_include_paths_;
    std::unordered_set<std::string> system_library_paths_;

    Profiles profiles_;

    static Result<void> parse_features(
        const toml::value& data,
        std::shared_ptr<Package> package);

    Result<void> parse_dependencies(
        const toml::value& data,
        std::shared_ptr<Package> package);

    Result<void> parse_profile(
        const toml::value& data);

    Result<void> parse_builds(
        const toml::value& data,
        const std::string& package_version,
        const std::string& path);

    Result<void> search_and_parse_dependency(
        const std::string& package_name,
        const std::string& version);

    std::shared_ptr<Package> find_package(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt);

    void resolve_system_dependency(
        const std::string& package_name,
        std::shared_ptr<Package> package);

    void merge_profiles(
        const std::string& base_profile,
        const std::string& inherited_profile);

    Result<void> parse_muuk_toml(const std::string& path, bool is_base = false);

    tl::expected<void, std::string> resolve_dependencies(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt,
        std::optional<std::string> search_path = std::nullopt);

    Result<void> resolve_build_dependencies(const std::string& build_name);

    Result<void> merge_build_dependencies(
        const std::string& build_name,
        std::shared_ptr<Build> build,
        const Dependency& base_package_dep);

    Result<void> merge_resolved_dependencies(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt);

    static std::string format_dependencies(
        const DependencyVersionMap<toml::table>& dependencies,
        std::string section_name = "dependencies");

    static std::string format_dependencies(
        const DependencyVersionMap<std::shared_ptr<Dependency>>& dependencies,
        const std::string& section_name = "dependencies");

    static void parse_and_store_flag_categories(
        const toml::value& data,
        std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>& target,
        const std::vector<std::string>& flag_categories);
};

#endif // MUUK_PARSER_H
