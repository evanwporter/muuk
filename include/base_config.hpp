#pragma once

#include "toml11/find.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

// namespace muuk {
//     namespace lockgen {

struct Dependency {
    std::string git_url;
    std::string path;
    std::string version;
    std::unordered_set<std::string> enabled_features;
    bool system = false;
    std::vector<std::string> libs;

    void load(const toml::value& v) {
        if (v.is_string()) {
            version = v.as_string();
            return;
        }

        git_url = toml::find_or<std::string>(v, "git", "");
        path = toml::find_or<std::string>(v, "path", "");
        version = toml::find_or<std::string>(v, "version", "");

        auto enabled_feats = toml::find_or<std::vector<std::string>>(v, "features", {});
        enabled_features = std::unordered_set<std::string>(enabled_feats.begin(), enabled_feats.end());

        libs
            = toml::find_or<std::vector<std::string>>(v, "libs", {});
    }

    static Dependency from_toml(const toml::value& data) {
        Dependency dep;

        dep.load(data);

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

    void serialize(toml::value& out) const {
        if (!git_url.empty())
            out["git"] = git_url;
        if (!path.empty())
            out["path"] = path;
        if (!version.empty())
            out["version"] = version;

        if (!enabled_features.empty()) {
            toml::array features;
            for (const auto& feat : enabled_features)
                features.push_back(feat);
            out["features"] = features;
        }

        if (!libs.empty()) {
            toml::array lib_array;
            for (const auto& lib : libs)
                lib_array.push_back(lib);
            out["libs"] = lib_array;
        }
    }
};

struct source_file {
    std::string path;
    std::vector<std::string> cflags;
};

template <typename Derived>
struct BaseFields {
    std::vector<std::string> modules, sources, libs, include, defines;
    std::vector<std::string> cflags, cxxflags, aflags, lflags;
    std::unordered_map<std::string, Dependency> dependencies;

    static constexpr bool enable_modules = true;
    static constexpr bool enable_sources = true;
    static constexpr bool enable_include = true;
    static constexpr bool enable_defines = true;
    static constexpr bool enable_cflags = true;
    static constexpr bool enable_cxxflags = true;
    static constexpr bool enable_aflags = true;
    static constexpr bool enable_lflags = true;
    static constexpr bool enable_dependencies = true;
    static constexpr bool enable_libs = true;

    void load(const toml::value& v) {
        if constexpr (Derived::enable_modules)
            modules = toml::find_or<std::vector<std::string>>(v, "modules", {});
        if constexpr (Derived::enable_sources)
            sources = toml::find_or<std::vector<std::string>>(v, "sources", {});
        if constexpr (Derived::enable_include)
            include = toml::find_or<std::vector<std::string>>(v, "include", {});
        if constexpr (Derived::enable_defines)
            defines = toml::find_or<std::vector<std::string>>(v, "defines", {});
        if constexpr (Derived::enable_cflags)
            cflags = toml::find_or<std::vector<std::string>>(v, "cflags", {});
        if constexpr (Derived::enable_cxxflags)
            cxxflags = toml::find_or<std::vector<std::string>>(v, "cxxflags", {});
        if constexpr (Derived::enable_aflags)
            aflags = toml::find_or<std::vector<std::string>>(v, "aflags", {});
        if constexpr (Derived::enable_lflags)
            lflags = toml::find_or<std::vector<std::string>>(v, "lflags", {});
        if constexpr (Derived::enable_libs)
            libs = toml::find_or<std::vector<std::string>>(v, "libs", {});

        if constexpr (Derived::enable_dependencies) {
            if (v.contains("dependencies")) {
                for (const auto& [k, val] : toml::find<toml::table>(v, "dependencies")) {
                    Dependency dep;
                    dep.load(val);
                    dependencies[k] = dep;
                }
            }
        }
    }

    void serialize(toml::value& out) const {
        if constexpr (Derived::enable_modules)
            out["modules"] = modules;
        if constexpr (Derived::enable_sources)
            out["sources"] = sources;
        if constexpr (Derived::enable_include)
            out["include"] = include;
        if constexpr (Derived::enable_defines)
            out["defines"] = defines;
        if constexpr (Derived::enable_cflags)
            out["cflags"] = cflags;
        if constexpr (Derived::enable_cxxflags)
            out["cxxflags"] = cxxflags;
        if constexpr (Derived::enable_aflags)
            out["aflags"] = aflags;
        if constexpr (Derived::enable_lflags)
            out["lflags"] = lflags;
        if constexpr (Derived::enable_libs)
            out["libs"] = libs;

        if constexpr (Derived::enable_dependencies) {
            toml::value deps_out = toml::table {};
            for (const auto& [key, dep] : dependencies) {
                toml::value dep_val = toml::table {};
                dep.serialize(dep_val);
                deps_out[key] = dep_val;
            }
            out["dependencies"] = deps_out;
        }
    }

    // TODO append base path to includes
    void merge(const Derived& child_derived) {
        include.insert(include.end(), child_derived.include.begin(), child_derived.include.end());
        cflags.insert(cflags.end(), child_derived.cflags.begin(), child_derived.cflags.end());
        cxxflags.insert(cxxflags.end(), child_derived.cxxflags.begin(), child_derived.cxxflags.end());
        aflags.insert(aflags.end(), child_derived.aflags.begin(), child_derived.aflags.end());
        lflags.insert(lflags.end(), child_derived.lflags.begin(), child_derived.lflags.end());
        defines.insert(defines.end(), child_derived.defines.begin(), child_derived.defines.end());
        libs.insert(libs.end(), child_derived.libs.begin(), child_derived.libs.end());
    }

    std::vector<source_file> parse_sources(const toml::value& section) {
        if (section.contains("sources") && !section.at("sources").is_array()) {
            std::vector<source_file> temp_sources;
            for (const auto& src : section.at("sources").as_array()) {
                std::string source_entry = src.as_string();

                std::vector<std::string> extracted_cflags;
                std::string file_path;

                // Splitting "source.cpp CFLAGS" into file_path and flags
                size_t space_pos = source_entry.find(' ');
                if (space_pos != std::string::npos) {
                    file_path = source_entry.substr(0, space_pos);
                    std::string flags_str = source_entry.substr(space_pos + 1);

                    std::istringstream flag_stream(flags_str);
                    std::string flag;
                    while (flag_stream >> flag) {
                        extracted_cflags.push_back(flag);
                    }
                } else {
                    file_path = source_entry;
                }

                temp_sources.emplace_back(
                    file_path,
                    extracted_cflags);
            }

            return temp_sources;
        }
        return {}; // Return empty vector if no sources are defined
    }
};

struct CompilerConfig : BaseFields<CompilerConfig> {
    void load(const toml::value& v) {
        BaseFields::load(v);
    }
};

struct PlatformConfig : BaseFields<PlatformConfig> {
    void load(const toml::value& v) {
        BaseFields::load(v);
    }
};

struct Compilers {
    CompilerConfig clang, gcc, msvc;

    void load(const toml::value& v) {
        if (v.contains("clang"))
            clang.load(v.at("clang"));

        if (v.contains("clang"))
            gcc.load(v.at("clang"));

        if (v.contains("clang"))
            msvc.load(v.at("clang"));
    }

    void merge(const Compilers& other) {
        clang.merge(other.clang);
        gcc.merge(other.gcc);
        msvc.merge(other.msvc);
    }

    void serialize(toml::value& out) const {
        toml::value compiler;
        clang.serialize(compiler["clang"]);
        gcc.serialize(compiler["gcc"]);
        msvc.serialize(compiler["msvc"]);
        out["compiler"] = compiler;
    }
};

struct Platforms {
    PlatformConfig windows, linux, apple;

    void load(const toml::value& v) {
        if (v.contains("windows"))
            windows.load(v.at("windows"));

        if (v.contains("linux"))
            linux.load(v.at("linux"));

        if (v.contains("apple"))
            apple.load(v.at("apple"));
    }

    void merge(const Platforms& other) {
        windows.merge(other.windows);
        linux.merge(other.linux);
        apple.merge(other.apple);
    }

    void serialize(toml::value& out) const {
        toml::value platform;
        windows.serialize(platform["windows"]);
        linux.serialize(platform["linux"]);
        apple.serialize(platform["apple"]);
        out["platform"] = platform;
    }
};

template <typename Derived>
struct BaseConfig : public BaseFields<Derived> {
    Compilers compilers;
    Platforms platforms;

    toml::value raw_compiler;
    toml::value raw_platform;

    static constexpr bool enable_compilers = true;
    static constexpr bool enable_platforms = true;

    void load(const toml::value& v) {
        BaseFields<Derived>::load(v);

        if constexpr (Derived::enable_compilers) {
            if (v.contains("compiler")) {
                raw_compiler = toml::find(v, "compiler");
                compilers.load(raw_compiler);
            }
        }

        if constexpr (Derived::enable_platforms) {
            if (v.contains("platform")) {
                raw_platform = toml::find(v, "platform");
                platforms.load(raw_platform);
            }
        }
    }

    void merge(const Derived& other) {
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

struct Build : BaseConfig<Build> {
    std::vector<std::string> profiles;
    // nothing disabled
};

struct Library : BaseConfig<Library> {
    std::string name;
    std::string version;

    static constexpr bool enable_compilers = false;
    static constexpr bool enable_platforms = false;
    static constexpr bool enable_dependencies = false;

    struct External {
        std::string type, path;
        std::vector<std::string> args;
        std::vector<std::string> outputs;

        void load(const toml::value& v) {
            type = toml::find_or<std::string>(v, "type", "");
            path = toml::find_or<std::string>(v, "path", "");
            args = toml::find_or<std::vector<std::string>>(v, "args", {});
            outputs = toml::find_or<std::vector<std::string>>(v, "outputs", {});
        }

        void serialize(toml::value& out) const {
            // BaseConfig<Library>::serialize(out);

            toml::value external_section = toml::table {};
            if (!type.empty())
                external_section["type"] = type;
            if (!path.empty())
                external_section["path"] = path;
            if (!args.empty())
                external_section["args"] = args;
            if (!outputs.empty())
                external_section["outputs"] = outputs;

            out["external"] = external_section;
        }
    };

    External external;

    void load(std::string name_, std::string version_, const toml::value& v) {
        name = name_;
        version = version_;

        BaseConfig<Library>::load(v);
        if (v.contains("external")) {
            external.load(toml::find(v, "external"));
        }
    }

    void serialize(toml::value& out) const {
        out["name"] = name;
        out["version"] = version;

        BaseConfig<Library>::serialize(out);

        toml::value external_section = toml::table {};
        if (!external.type.empty())
            external_section["type"] = external.type;
        if (!external.path.empty())
            external_section["path"] = external.path;
        if (!external.args.empty())
            external_section["args"] = external.args;
        if (!external.outputs.empty())
            external_section["outputs"] = external.outputs;

        // external_section.as_table_fmt().fmt = toml::table_format::oneline;

        out["external"] = external_section;
    }
};

struct ProfileConfig : BaseConfig<ProfileConfig> {
    std::string name;
    std::vector<std::string> inherits;

    void load(const toml::value& v, const std::string& profile_name) {
        BaseConfig<ProfileConfig>::load(v);
        name = profile_name;
        inherits = toml::find_or<std::vector<std::string>>(v, "inherits", {});
    }
};
// } // namespace lockgen
// } // namespace muuk