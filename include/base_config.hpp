#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "logger.h"
#include "muuk.h"
#include "util.h"

enum class LinkType {
    STATIC,
    SHARED
};

using Profiles = std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>;

struct Feature {
    std::unordered_set<std::string> defines;
    std::unordered_set<std::string> undefines;
    std::unordered_set<std::string> dependencies;
};

class PackageType {
public:
    enum class Type {
        LIBRARY,
        BUILD
    };

    static const PackageType LIBRARY;
    static const PackageType BUILD;

    explicit PackageType(Type type);

    static std::string to_string(Type type);
    std::string to_string() const;
    static PackageType from_string(const std::string& typeStr);

    bool operator==(const PackageType& other) const;
    bool operator!=(const PackageType& other) const;

private:
    Type type_;
};

struct Dependency {
    std::string name;
    std::string git_url;
    std::string path;
    std::string version;
    std::unordered_set<std::string> enabled_features;
    bool system = false;
    std::vector<std::string> libs;

    Result<void> load(const std::string name_, const toml::value& v);
    static Dependency from_toml(const toml::value& data);
    Result<void> serialize(toml::value& out) const;
};

struct source_file {
    std::string path;
    std::vector<std::string> cflags;

    toml::value serialize() const;

    source_file(std::string p, const std::vector<std::string>& f) :
        path(std::move(p)), cflags(f) { }
};

using module_file = source_file;

template <typename Derived>
struct BaseFields {
    std::vector<source_file> sources;
    std::vector<module_file> modules;
    std::unordered_set<std::string> libs, include, defines, undefines;
    std::unordered_set<std::string> cflags, cxxflags, aflags, lflags;
    DependencyVersionMap<Dependency> dependencies;

    static constexpr bool enable_modules = true;
    static constexpr bool enable_sources = true;
    static constexpr bool enable_include = true;
    static constexpr bool enable_defines = true;
    static constexpr bool enable_undefines = true;
    static constexpr bool enable_cflags = true;
    static constexpr bool enable_cxxflags = true;
    static constexpr bool enable_aflags = true;
    static constexpr bool enable_lflags = true;
    static constexpr bool enable_dependencies = true;
    static constexpr bool enable_libs = true;

    void load(const toml::value& v, const std::string& base_path) {
        if constexpr (Derived::enable_modules)
            modules = parse_sources(v, base_path, "modules");
        if constexpr (Derived::enable_sources)
            sources = parse_sources(v, base_path);
        if constexpr (Derived::enable_include) {
            auto raw_includes = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "include", {});
            for (const auto& inc : raw_includes)
                include.insert(util::file_system::to_linux_path((std::filesystem::path(base_path) / inc).lexically_normal().string()));
        }
        if constexpr (Derived::enable_defines)
            defines = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "defines", {});
        if constexpr (Derived::enable_undefines)
            defines = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "undefines", {});
        if constexpr (Derived::enable_cflags)
            cflags = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "cflags", {});
        if constexpr (Derived::enable_cxxflags)
            cxxflags = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "cxxflags", {});
        if constexpr (Derived::enable_aflags)
            aflags = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "aflags", {});
        if constexpr (Derived::enable_lflags)
            lflags = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "lflags", {});
        if constexpr (Derived::enable_libs)
            libs = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "libs", {});

        if constexpr (Derived::enable_dependencies) {
            if (v.contains("dependencies")) {
                for (const auto& [k, val] : toml::find<toml::table>(v, "dependencies")) {
                    Dependency dep;
                    dep.load(k, val);
                    dependencies[k][dep.version] = dep;
                }
            }
        }
    }

    void serialize(toml::value& out) const {
        if constexpr (Derived::enable_modules) {
            toml::array module_array;
            for (const auto& s : expand_glob_sources(modules))
                module_array.push_back(s.serialize());
            if (!module_array.empty())
                out["modules"] = module_array;
        }
        if constexpr (Derived::enable_sources) {
            toml::array src_array;
            for (const auto& s : expand_glob_sources(sources))
                src_array.push_back(s.serialize());
            if (!src_array.empty())
                out["sources"] = src_array;
        }
        if constexpr (Derived::enable_include)
            if (!include.empty())
                out["include"] = include;
        if constexpr (Derived::enable_defines)
            if (!defines.empty())
                out["defines"] = defines;
        if constexpr (Derived::enable_undefines)
            if (!undefines.empty())
                out["undefines"] = undefines;
        if constexpr (Derived::enable_cflags)
            if (!cflags.empty())
                out["cflags"] = cflags;
        if constexpr (Derived::enable_cxxflags)
            if (!cxxflags.empty())
                out["cxxflags"] = cxxflags;
        if constexpr (Derived::enable_aflags)
            if (!aflags.empty())
                out["aflags"] = aflags;
        if constexpr (Derived::enable_lflags)
            if (!lflags.empty())
                out["lflags"] = lflags;
        if constexpr (Derived::enable_libs)
            if (!libs.empty())
                out["libs"] = libs;
        if constexpr (Derived::enable_dependencies) {
            toml::value deps_out = toml::table {};
            // for (const auto& [key, dep] : dependencies) {
            //     toml::value dep_val = toml::table {};
            //     dep.serialize(dep_val);
            //     deps_out[key] = dep_val;
            // }
            out["dependencies"] = deps_out;
        }
    }

    void merge(const Derived& other) {
        using util::array_ops::merge;
        merge(include, other.include);
        merge(cflags, other.cflags);
        merge(cxxflags, other.cxxflags);
        merge(aflags, other.aflags);
        merge(lflags, other.lflags);
        merge(defines, other.defines);
        merge(undefines, other.undefines);
        merge(libs, other.libs);
    }

    std::vector<source_file> parse_sources(const toml::value& section, const std::string& base_path, const std::string& key = "sources") {
        std::vector<source_file> temp_sources;
        if (!section.contains(key))
            return temp_sources;

        for (const auto& src : section.at(key).as_array()) {
            std::string source_entry = src.as_string();

            std::vector<std::string> extracted_cflags;
            std::string file_path;

            size_t space_pos = source_entry.find(' ');
            if (space_pos != std::string::npos) {
                file_path = source_entry.substr(0, space_pos);
                std::string flags_str = source_entry.substr(space_pos + 1);

                std::istringstream flag_stream(flags_str);
                std::string flag;
                while (flag_stream >> flag)
                    extracted_cflags.push_back(flag);
            } else {
                file_path = source_entry;
            }

            std::filesystem::path full_path = std::filesystem::path(base_path) / file_path;
            temp_sources.emplace_back(util::file_system::to_linux_path(full_path.lexically_normal().string()), extracted_cflags);
        }

        return temp_sources;
    }

    static std::vector<source_file> expand_glob_sources(const std::vector<source_file>& input_sources) {
        std::vector<source_file> expanded;

        for (const auto& s : input_sources) {
            try {
                std::vector<std::filesystem::path> globbed_paths = glob::glob(s.path);
                for (const auto& path : globbed_paths) {
                    expanded.emplace_back(util::file_system::to_linux_path(path.string()), s.cflags);
                }
            } catch (const std::exception& e) {
                muuk::logger::warn("Error while globbing '{}': {}", s.path, e.what());
            }
        }

        return expanded;
    }
};

struct CompilerConfig : BaseFields<CompilerConfig> {
    void load(const toml::value& v, const std::string& base_path);
};

struct PlatformConfig : BaseFields<PlatformConfig> {
    void load(const toml::value& v, const std::string& base_path);
};

struct Compilers {
    CompilerConfig clang, gcc, msvc;
    void load(const toml::value& v, const std::string& base_path);
    void merge(const Compilers& other);
    void serialize(toml::value& out) const;
};

struct Platforms {
    PlatformConfig windows, linux, apple;
    void load(const toml::value& v, const std::string& base_path);
    void merge(const Platforms& other);
    void serialize(toml::value& out) const;
};

template <typename Derived>
struct BaseConfig : public BaseFields<Derived> {
    Compilers compilers;
    Platforms platforms;
    toml::value raw_compiler;
    toml::value raw_platform;

    static constexpr bool enable_compilers = true;
    static constexpr bool enable_platforms = true;

    void load(const toml::value& v, const std::string& base_path) {
        BaseFields<Derived>::load(v, base_path);

        if constexpr (Derived::enable_compilers)
            if (v.contains("compiler"))
                compilers.load(toml::find(v, "compiler"), base_path);

        if constexpr (Derived::enable_platforms)
            if (v.contains("platform"))
                platforms.load(toml::find(v, "platform"), base_path);
    }

    void merge(const Derived& other) { // cppcheck-suppress duplInheritedMember
        BaseFields<Derived>::merge(other);

        if constexpr (Derived::enable_compilers) {
            compilers.clang.merge(other.compilers.clang);
            compilers.gcc.merge(other.compilers.gcc);
            compilers.msvc.merge(other.compilers.msvc);
        }

        if constexpr (Derived::enable_platforms) {
            platforms.windows.merge(other.platforms.windows);
            platforms.linux.merge(other.platforms.linux);
            platforms.apple.merge(other.platforms.apple);
        }
    }

    void serialize(toml::value& out) const {
        BaseFields<Derived>::serialize(out);

        if constexpr (Derived::enable_compilers)
            compilers.serialize(out);
        if constexpr (Derived::enable_platforms)
            platforms.serialize(out);
    }
};

struct ProfileConfig : BaseConfig<ProfileConfig> {
    std::string name;
    std::vector<std::string> inherits;

    void load(const toml::value& v, const std::string& profile_name, const std::string& base_path);
};

struct Library : BaseConfig<Library> {
    std::string name;
    std::string version;
    std::unordered_set<std::string> profiles;

    static constexpr bool enable_compilers = false;
    static constexpr bool enable_platforms = false;
    static constexpr bool enable_dependencies = false;

    struct External {
        std::string type, path;
        std::vector<std::string> args;
        std::vector<std::string> outputs;

        void load(const toml::value& v);
        void serialize(toml::value& out) const;
    };

    External external;

    void load(const std::string& name_, const std::string& version_, const std::string& base_path_, const toml::value& v);
    void serialize(toml::value& out) const;
};

class Package {
public:
    Package(
        const std::string& name,
        const std::string& version,
        const std::string& base_path,
        const PackageType package_type);

    void merge(const Package& child_pkg);

    std::string serialize() const;
    void enable_features(const std::unordered_set<std::string>& feature_set);

    std::string name;
    std::string version;
    std::string base_path;
    PackageType package_type; // "library" or "build"

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
    LinkType link_type = LinkType::STATIC;

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

struct Build : BaseConfig<Build> {
    std::unordered_set<std::string> profiles;
    std::unordered_set<std::shared_ptr<Dependency>> all_dependencies_array;

    static constexpr bool enable_compilers = false;
    static constexpr bool enable_platforms = false;

    void merge(const Package& package);
    Result<void> serialize(toml::value& out) const;
    void load(const toml::value& v, const std::string& base_path);
};
