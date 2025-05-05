#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "logger.hpp"
#include "muuk.hpp"
#include "toml_ext.hpp"
#include "util.hpp"

enum class LinkType {
    STATIC,
    SHARED
};

struct Feature {
    std::unordered_set<std::string> defines;
    std::unordered_set<std::string> undefines;
    std::unordered_set<std::string> dependencies;
};

/// Conditionally sets a key in a TOML table if the container is not empty.
template <typename T>
static inline void maybe_set(toml::value& out, const char* key, const T& container) {
    if (!container.empty())
        out[key] = container;
}

/// Forces a TOML table to be serialized in a single line.
inline void force_oneline(toml::value& v) {
    if (v.is_table())
        v.as_table_fmt().fmt = toml::table_format::oneline;
}

/// Conditionally serializes a member field to a TOML table based on a compile-time flag.
#define MAYBE_SET_FIELD(enable_macro, field_name) \
    if constexpr (Derived::enable_##enable_macro) \
    maybe_set(out, #field_name, field_name)

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
            auto raw_includes = toml::try_find_or<std::unordered_set<std::string>>(v, "include", {});
            for (const auto& inc : raw_includes)
                include.insert(util::file_system::to_linux_path((std::filesystem::path(base_path) / inc).lexically_normal().string()));
        }
        if constexpr (Derived::enable_defines)
            defines = toml::try_find_or<std::unordered_set<std::string>>(v, "defines", {});
        if constexpr (Derived::enable_undefines)
            undefines = toml::try_find_or<std::unordered_set<std::string>>(v, "undefines", {});
        if constexpr (Derived::enable_cflags)
            cflags = toml::try_find_or<std::unordered_set<std::string>>(v, "cflags", {});
        if constexpr (Derived::enable_cxxflags)
            cxxflags = toml::try_find_or<std::unordered_set<std::string>>(v, "cxxflags", {});
        if constexpr (Derived::enable_aflags)
            aflags = toml::try_find_or<std::unordered_set<std::string>>(v, "aflags", {});
        if constexpr (Derived::enable_lflags)
            lflags = toml::try_find_or<std::unordered_set<std::string>>(v, "lflags", {});
        if constexpr (Derived::enable_libs)
            libs = toml::try_find_or<std::unordered_set<std::string>>(v, "libs", {});

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
            maybe_set(out, "modules", module_array);
        }
        if constexpr (Derived::enable_sources) {
            toml::array src_array;
            for (const auto& s : expand_glob_sources(sources))
                src_array.push_back(s.serialize());
            maybe_set(out, "sources", src_array);
        }

        MAYBE_SET_FIELD(include, include);
        MAYBE_SET_FIELD(defines, defines);
        MAYBE_SET_FIELD(undefines, undefines);
        MAYBE_SET_FIELD(cflags, cflags);
        MAYBE_SET_FIELD(cxxflags, cxxflags);
        MAYBE_SET_FIELD(aflags, aflags);
        MAYBE_SET_FIELD(lflags, lflags);
        MAYBE_SET_FIELD(libs, libs);

        // TODO: Uncomment when full logic is implemented
        // if constexpr (Derived::enable_dependencies) {
        //     toml::value deps_out = toml::table {};
        //     // for (const auto& [key, dep] : dependencies) {
        //     //     toml::value dep_val = toml::table {};
        //     //     dep.serialize(dep_val);
        //     //     deps_out[key] = dep_val;
        //     // }
        //     out["dependencies"] = deps_out;
        // }
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
            temp_sources.emplace_back(
                util::file_system::to_linux_path(full_path.lexically_normal().string()),
                extracted_cflags);
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

    void load(
        const toml::value& v,
        const std::string& profile_name,
        const std::string& base_path);

    void serialize(toml::value& out) const;
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
    void serialize(toml::value& out, Platforms platforms, Compilers compilers) const;
};
