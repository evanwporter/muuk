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
};

template <typename Derived>
struct BaseFields {
    std::vector<std::string> modules, sources, libs, include, defines;
    std::vector<std::string> cflags, cxxflags, aflags, lflags;
    std::unordered_map<std::string, Dependency> dependencies;

    static constexpr bool disable_modules = false;
    static constexpr bool disable_sources = false;
    static constexpr bool disable_include = false;
    static constexpr bool disable_defines = false;
    static constexpr bool disable_cflags = false;
    static constexpr bool disable_cxxflags = false;
    static constexpr bool disable_aflags = false;
    static constexpr bool disable_lflags = false;
    static constexpr bool disable_dependencies = false;
    static constexpr bool disable_libs = false;

    void load(const toml::value& v) {
        if constexpr (!Derived::disable_modules)
            modules = toml::find_or<std::vector<std::string>>(v, "modules", {});
        if constexpr (!Derived::disable_sources)
            sources = toml::find_or<std::vector<std::string>>(v, "sources", {});
        if constexpr (!Derived::disable_include)
            include = toml::find_or<std::vector<std::string>>(v, "include", {});
        if constexpr (!Derived::disable_defines)
            defines = toml::find_or<std::vector<std::string>>(v, "defines", {});
        if constexpr (!Derived::disable_cflags)
            cflags = toml::find_or<std::vector<std::string>>(v, "cflags", {});
        if constexpr (!Derived::disable_cxxflags)
            cxxflags = toml::find_or<std::vector<std::string>>(v, "cxxflags", {});
        if constexpr (!Derived::disable_aflags)
            aflags = toml::find_or<std::vector<std::string>>(v, "aflags", {});
        if constexpr (!Derived::disable_lflags)
            lflags = toml::find_or<std::vector<std::string>>(v, "lflags", {});
        if constexpr (!Derived::disable_libs)
            libs = toml::find_or<std::vector<std::string>>(v, "libs", {});

        if constexpr (!Derived::disable_dependencies) {
            if (v.contains("dependencies")) {
                for (const auto& [k, val] : toml::find<toml::table>(v, "dependencies")) {
                    Dependency dep;
                    dep.load(val);
                    dependencies[k] = dep;
                }
            }
        }
    }

    // void merge(const Derived& child_derived) {
    //     for (const auto& path : child_derived.include)
    //         include.insert(path);

    //     cflags.insert(child_derived.cflags.begin(), child_derived.cflags.end());
    //     cxxflags.insert(child_derived.cxxflags.begin(), child_derived.cxxflags.end());
    //     aflags.insert(child_derived.aflags.begin(), child_derived.aflags.end());
    //     lflags.insert(child_derived.lflags.begin(), child_derived.lflags.end());

    //     defines.insert(child_derived.defines.begin(), child_derived.defines.end());
    // }
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
        clang.load(toml::find_or(v, "clang", {}));
        gcc.load(toml::find_or(v, "gcc", {}));
        msvc.load(toml::find_or(v, "msvc", {}));
    }
};

struct Platforms {
    PlatformConfig windows, linux, apple;

    void load(const toml::value& v) {
        windows.load(toml::find_or(v, "windows", {}));
        linux.load(toml::find_or(v, "linux", {}));
        apple.load(toml::find_or(v, "apple", {}));
    }
};

template <typename Derived>
struct BaseConfig : public BaseFields<Derived> {
    Compilers compilers;
    Platforms platforms;

    toml::value raw_compiler;
    toml::value raw_platform;

    static constexpr bool disable_compilers = false;
    static constexpr bool disable_platforms = false;

    void load(const toml::value& v) {
        BaseFields<Derived>::load(v);

        if constexpr (!Derived::disable_compilers) {
            if (v.contains("compiler")) {
                raw_compiler = toml::find(v, "compiler");
                compilers.load(raw_compiler);
            }
        }

        if constexpr (!Derived::disable_platforms) {
            if (v.contains("platform")) {
                raw_platform = toml::find(v, "platform");
                platforms.load(raw_platform);
            }
        }
    }

    // void merge(const Derived& other) {
    //     BaseFields<Derived>::merge(other);

    //     if constexpr (!Derived::disable_compilers) {
    //         compilers.clang.merge(other.compilers.clang);
    //         compilers.gcc.merge(other.compilers.gcc);
    //         compilers.msvc.merge(other.compilers.msvc);
    //     }

    //     if constexpr (!Derived::disable_platforms) {
    //         platforms.windows.merge(other.platforms.windows);
    //         platforms.linux.merge(other.platforms.linux);
    //         platforms.apple.merge(other.platforms.apple);
    //     }
    // }
};

struct Build : BaseConfig<Build> {
    std::vector<std::string> profiles;
    // nothing disabled
};

struct Library : BaseConfig<Library> {
    static constexpr bool disable_compilers = true;
    static constexpr bool disable_platforms = true;
    static constexpr bool disable_dependencies = true;

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
    };

    External external;

    void load(const toml::value& v) {
        BaseConfig<Library>::load(v);
        if (v.contains("external")) {
            external.load(toml::find(v, "external"));
        }
    }

    // void merge(const Library& child_lib) {
    //     for (const auto& path : child_lib.include)
    //         include.insert(path);

    //     cflags.insert(child_lib.cflags.begin(), child_lib.cflags.end());
    //     cxxflags.insert(child_lib.cxxflags.begin(), child_lib.cxxflags.end());
    //     aflags.insert(child_lib.aflags.begin(), child_lib.aflags.end());
    //     lflags.insert(child_lib.lflags.begin(), child_lib.lflags.end());

    //     defines.insert(child_lib.defines.begin(), child_lib.defines.end());
    // }
};

struct Profile : BaseConfig<Profile> {
    std::string name;
    std::vector<std::string> inherits;

    void load(const toml::value& v, const std::string& profile_name) {
        BaseConfig<Profile>::load(v);
        name = profile_name;
        inherits = toml::find_or<std::vector<std::string>>(v, "inherits", {});
    }
};
// } // namespace lockgen
// } // namespace muuk