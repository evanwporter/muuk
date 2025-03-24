#ifndef MUUK_LOCK_GEN_H
#define MUUK_LOCK_GEN_H

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

#include "compiler.hpp"
#include "muuk.h"
#include "rustify.hpp"

enum class LinkType {
    STATIC,
    SHARED
};

// enum class PackageType {
//     LIBRARY,
//     BUILD
// };

class PackageType {
public:
    enum class Type {
        LIBRARY,
        BUILD
    };

    static const PackageType LIBRARY;
    static const PackageType BUILD;

    explicit PackageType(Type type) :
        type_(type) { }

    static std::string to_string(Type type) {
        if (type == Type::BUILD) {
            return "build";
        } else {
            return "library";
        }
    }

    std::string to_string() const {
        return to_string(type_);
    }

    // Convert string to PackageType
    static PackageType from_string(const std::string& typeStr) {
        if (typeStr == "build") {
            return PackageType(Type::BUILD);
        } else {
            return PackageType(Type::LIBRARY);
        }
    }

    bool operator==(const PackageType& other) const { return type_ == other.type_; }
    bool operator!=(const PackageType& other) const { return type_ != other.type_; }

private:
    Type type_;
};

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
    std::unordered_set<std::string> libs;

    static Dependency from_toml(const toml::value& data) {
        Dependency dep;

        if (data.is_string()) {
            dep.version = data.as_string();
            return dep;
        }

        if (data.contains("git"))
            dep.git_url = data.at("git").as_string();

        if (data.contains("path"))
            dep.path = data.at("path").as_string();

        if (data.contains("version"))
            dep.version = data.at("version").as_string();

        if (data.contains("features"))
            for (const auto& feature : data.at("features").as_array())
                dep.enabled_features.insert(feature.as_string());

        if (data.contains("libs"))
            for (const auto& lib : data.at("libs").as_array()) {
                dep.libs.insert(lib.as_string());
            }

        return dep;
    }

    toml::value to_toml() const {
        toml::value dep_table = toml::table {};

        if (!git_url.empty())
            dep_table["git"] = git_url;
        if (!path.empty())
            dep_table["path"] = path;
        if (!version.empty())
            dep_table["version"] = version;

        if (!enabled_features.empty()) {
            toml::array features;
            for (const auto& feat : enabled_features)
                features.push_back(feat);
            dep_table["features"] = features;
        }

        if (!libs.empty()) {
            toml::array lib_array;
            for (const auto& lib : libs)
                lib_array.push_back(lib);
            dep_table["libs"] = lib_array;
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
        const PackageType package_type);

    void add_include_path(const std::string& path);

    void merge(const Package& child_pkg);

    std::string serialize() const;
    void enable_features(const std::unordered_set<std::string>& feature_set);

    std::string name;
    std::string version;
    std::string base_path;
    PackageType package_type; // "library" or "build"

    std::string source;

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
    static void serialize_dependencies(
        std::ostringstream& toml_stream,
        const DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_,
        const std::string section_name);
};

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

    std::shared_ptr<Package> base_package_;

    // Meant to be a general reference for packages to refer to
    DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_;

    std::unordered_set<std::string> visited;

    // <Package, Version> vector array
    std::vector<std::pair<std::string, std::string>> resolved_order_;

    std::unordered_set<std::string> system_include_paths_;
    std::unordered_set<std::string> system_library_paths_;

    Profiles profiles_;

    void parse_flags(
        const toml::value& profile_data,
        const std::string& profile_name,
        const std::string& flag_type);

    static Result<void> parse_library(
        const toml::value& data,
        std::shared_ptr<Package> package);

    static Result<void> parse_platform(
        const toml::value& data,
        std::shared_ptr<Package> package);

    static Result<void> parse_compiler(
        const toml::value& data,
        std::shared_ptr<Package> package);

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
