#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"
#include "lockgen/config/build.hpp"
#include "lockgen/config/package.hpp"
#include "muuk.hpp"
#include "rustify.hpp"

// { Dependency { Versioning { Dependency Info } } }
/// Maps dependencies to their versioning information and associated dependency details.
typedef DependencyVersionMap<Dependency> DependencyInfoMap;

/// Maps dependencies to their respective versions and associated packages.
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
    std::unordered_map<std::string, ProfileConfig> profiles_config_;

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

    /// Searches for the specified package in the dependency folder and parses its muuk.toml file if found.
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

    /// Parse a single muuk.toml file representing a package
    Result<void> parse_muuk_toml(const std::string& path, bool is_base = false);

    Result<void> resolve_dependencies(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt,
        std::optional<std::string> search_path = std::nullopt);

    Result<void> locate_and_parse_package(
        const std::string& package_name,
        std::optional<std::string> version,
        std::shared_ptr<Package>& package,
        const std::optional<std::string> search_path);

    Result<void> resolve_build_dependencies(const std::string& build_name);

    Result<void> merge_build_dependencies(
        const std::string& build_name,
        std::shared_ptr<Build> build,
        const Dependency& base_package_dep);

    /// Merges the resolved dependencies of a package into the package itself.
    Result<void> merge_resolved_dependencies(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt);

    static void parse_and_store_flag_categories(
        const toml::value& data,
        std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>& target,
        const std::vector<std::string>& flag_categories);

    void propagate_profiles();

    void propagate_profiles_downward(
        Package& package,
        const std::unordered_set<std::string>& inherited_profiles);

    void generate_gitignore();
};

#endif // MUUK_LOCK_GEN_H
