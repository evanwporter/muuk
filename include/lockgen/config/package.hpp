#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"
#include "lockgen/config/library.hpp"
#include "muuk.hpp"

class Package {
public:
    Package(
        const std::string& name,
        const std::string& version,
        const std::string& base_path);

    void merge(const Package& child_pkg);

    std::string serialize() const;
    void enable_features(const std::unordered_set<std::string>& feature_set);

    std::string name;
    std::string version;
    std::string base_path;

    /// Git URL or local path
    std::string source;

    /// Map of dependencies by package name and version.
    DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_;

    std::unordered_set<std::shared_ptr<Dependency>> all_dependencies_array;

    /// Features enabled automatically unless overridden.
    std::unordered_set<std::string> default_features;

    /// Map of available features and their properties (defines, deps, etc.).
    std::unordered_map<std::string, Feature> features;

    /// Preferred link type for the package.
    muuk::LinkType link_type = muuk::LinkType::STATIC;

    /// Compiler-specific settings parsed from `[compiler]`.
    Compilers compilers_config;

    /// Platform-specific settings parsed from `[platform]`.
    Platforms platforms_config;

    /// Library build settings (cflags, sources, modules, etc.) `[library]`.
    Library library_config;

    /// (BUILD ONLY) profiles parsed from [profile.*] sections.
    std::unordered_map<std::string, ProfileConfig> profiles_config;

private:
    static void serialize_dependencies(
        std::ostringstream& toml_stream,
        const DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_,
        const std::string section_name);
};
