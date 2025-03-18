#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include "muukfiler.h"
#include "rustify.hpp"

enum class LinkType {
    STATIC,
    SHARED
};

enum class PackageType {
    LIBRARY,
    BUILD
};

// class PackageType {
// public:
//     enum class Type {
//         LIBRARY,
//         BUILD
//     };

//     static const PackageType LIBRARY;
//     static const PackageType BUILD;

//     explicit PackageType(Type type) :
//         type_(type) { }

//     static std::string to_string(Type type) {
//         return std::string(magic_enum::enum_name(type));
//     }

//     std::string to_string() const {
//         return to_string(type_);
//     }

//     // Convert string to PackageType
//     static PackageType from_string(const std::string& typeStr) {
//         auto result = magic_enum::enum_cast<Type>(typeStr);
//         return PackageType(result.value_or(Type::LIBRARY)); // Default to LIBRARY if not found
//     }

//     bool operator==(const PackageType& other) const { return type_ == other.type_; }
//     bool operator!=(const PackageType& other) const { return type_ != other.type_; }

// private:
//     Type type_;
// };

// const PackageType PackageType::LIBRARY(PackageType::Type::LIBRARY);
// const PackageType PackageType::BUILD(PackageType::Type::BUILD);

typedef std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>
    Profiles;

struct Feature {
    std::unordered_set<std::string> defines;
    std::unordered_set<std::string> dependencies;
};

struct Dependency {
    std::string git_url;
    std::string path;
    std::string version;
    std::unordered_set<std::string> enabled_features;
    bool system = false;

    static Dependency from_toml(const toml::table& data) {
        Dependency dep;

        if (data.contains("git") && data["git"].is_string()) {
            dep.git_url = *data["git"].value<std::string>();
        }
        if (data.contains("path") && data["path"].is_string()) {
            dep.path = *data["path"].value<std::string>();
        }
        if (data.contains("version") && data["version"].is_string()) {
            dep.version = *data["version"].value<std::string>();
        }
        if (data.contains("features") && data["features"].is_array()) {
            for (const auto& feature : *data["features"].as_array()) {
                if (feature.is_string()) {
                    dep.enabled_features.insert(*feature.value<std::string>());
                }
            }
        }

        return dep;
    }

    toml::table to_toml() const {
        toml::table dep_table;
        if (!git_url.empty())
            dep_table.insert("git", git_url);
        if (!path.empty())
            dep_table.insert("path", path);
        if (!version.empty())
            dep_table.insert("version", version);
        if (!enabled_features.empty()) {
            toml::array feature_array;
            for (const auto& feat : enabled_features) {
                feature_array.push_back(feat);
            }
            dep_table.insert("features", feature_array);
        }
        return dep_table;
    }
};

// { Dependency { Versioning { Dependency Info } } }
typedef DependencyVersionMap<Dependency> DependencyInfoMap;

class Package {
public:
    Package(
        const std::string& name,
        const std::string& version,
        const std::string& base_path,
        const std::string& package_type);

    void add_include_path(const std::string& path);

    void merge(const Package& child_pkg);

    std::string serialize() const;
    void enable_features(const std::unordered_set<std::string>& feature_set);

    std::string name;
    std::string version;
    std::string base_path;
    std::string package_type; // "library" or "build"

    std::unordered_set<std::string> include;
    std::unordered_set<std::string> system_include;
    std::unordered_set<std::string> cflags;
    std::unordered_set<std::string> defines;
    std::vector<std::pair<std::string, std::vector<std::string>>> sources;
    std::vector<std::string> modules;
    std::vector<std::string> libs;
    std::unordered_set<std::string> inherited_profiles;

    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> platform_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> compiler_;

    // Reference the dependency held in the MuukLockGen
    DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_;
    DependencyVersionMap<std::shared_ptr<Dependency>> deps_all_;

    std::unordered_set<std::string> deps;

    std::unordered_map<std::string, Feature> features;

    LinkType link_type = LinkType::STATIC;

private:
    static void serialize_dependencies(std::ostringstream& toml_stream, const DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_, const std::string section_name);
};

// { Dependency { Versioning { Package } } }
typedef DependencyVersionMap<std::shared_ptr<Package>> DependencyMap;

class MuukLockGenerator {
public:
    explicit MuukLockGenerator(const std::string& base_path);
    void generate_lockfile(const std::string& output_path);

private:
    std::string base_path_;

    DependencyMap resolved_packages;
    std::unordered_map<std::string, std::shared_ptr<Package>> builds;

    std::shared_ptr<Package> base_package_;

    std::shared_ptr<spdlog::logger> logger_;

    // Meant to be a general reference for packages to refer to
    DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_;

    std::unordered_set<std::string> visited;

    // <Package, Version> vector array
    std::vector<std::pair<std::string, std::string>> resolved_order_;

    std::unordered_set<std::string> system_include_paths_;
    std::unordered_set<std::string> system_library_paths_;

    Profiles profiles_;

    void parse_flags(
        const toml::table& profile_data,
        const std::string& profile_name,
        const std::string& flag_type);
    static Result<void> parse_library(
        const toml::table& data,
        std::shared_ptr<Package> package);
    static Result<void> parse_platform(
        const toml::table& data,
        std::shared_ptr<Package> package);
    static Result<void> parse_compiler(
        const toml::table& data,
        std::shared_ptr<Package> package);
    static Result<void> parse_features(
        const toml::table& data,
        std::shared_ptr<Package> package);
    Result<void> parse_dependencies(
        const toml::table& data,
        std::shared_ptr<Package> package);
    Result<void> parse_profile(const toml::table& data);
    Result<void> parse_builds(
        const toml::table& data,
        const std::string& package_version,
        const std::string& path);

    void search_and_parse_dependency(
        const std::string& package_name,
        const std::string& version);

    std::optional<std::shared_ptr<Package>> find_package(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt);

    // TODO: Use or Remove
    void resolve_system_dependency(
        const std::string& package_name,
        std::optional<std::shared_ptr<Package>> package);

    void merge_profiles(
        const std::string& base_profile,
        const std::string& inherited_profile);

    void parse_muuk_toml(const std::string& path, bool is_base = false);

    tl::expected<void, std::string> resolve_dependencies(
        const std::string& package_name,
        std::optional<std::string> version = std::nullopt,
        std::optional<std::string> search_path = std::nullopt);

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
        const toml::table& data,
        std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>& target,
        const std::vector<std::string>& flag_categories);
};

#endif // MUUK_PARSER_H
