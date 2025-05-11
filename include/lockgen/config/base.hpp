#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "compiler.hpp"
#include "muuk.hpp"
#include "opt_level.hpp"
#include "toml_ext.hpp"
#include "util.hpp"

/// Conditionally sets a key in a TOML table if the container is not empty.
template <typename T>
static inline void maybe_set(toml::value& out, const char* key, const T& container) {
    if (!container.empty())
        out[key] = container;
}

/// Conditionally serializes a member field to a TOML table based on a compile-time flag.
#define MAYBE_SET_FIELD(enable_macro, field_name) \
    if constexpr (Derived::enable_##enable_macro) \
    maybe_set(out, #field_name, field_name)

/// Serializes and globs a vector of source files to a TOML array.
#define SERIALIZE_GLOB_SOURCES(field)                    \
    if constexpr (Derived::enable_##field) {             \
        toml::array arr;                                 \
        for (const auto& s : expand_glob_sources(field)) \
            arr.push_back(s.serialize());                \
        maybe_set(out, #field, arr);                     \
    }

/// Serializes a vector of source files to a TOML array.
#define SERIALIZE_SOURCES(field)             \
    if constexpr (Derived::enable_##field) { \
        toml::array arr;                     \
        for (const auto& s : field)          \
            arr.push_back(s.serialize());    \
        maybe_set(out, #field, arr);         \
    }

#define LOAD_TOML_SET_FIELD(enable_macro, field_name) \
    if constexpr (Derived::enable_##enable_macro)     \
    field_name = toml::try_find_or<std::unordered_set<std::string>>(v, #field_name, {})

#define LOAD_TOML_SOURCES(enable_macro, field_name, key_name) \
    if constexpr (Derived::enable_##enable_macro)             \
    field_name = parse_sources(v, base_path, key_name)

#define LOAD_TOML_LIBS(enable_macro, field_name)  \
    if constexpr (Derived::enable_##enable_macro) \
    field_name = parse_libs(v, base_path)

namespace muuk {
    namespace lockgen {
        struct Feature {
            std::unordered_set<std::string> defines;
            std::unordered_set<std::string> undefines;
            std::unordered_set<std::string> dependencies;
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
            std::unordered_set<std::string> cflags;

            toml::value serialize() const;

            // source_file(std::string p, const std::vector<std::string>& f) :
            //     path(std::move(p)), cflags(f) { }
        };

        using module_file = source_file;

        struct lib_file {
            std::string path;
            std::vector<std::string> lflags;
            std::optional<Compiler> compiler;

            toml::value serialize() const;
            // TODO: Platform: if platform doesn't match then skip this lib
            // If compiler is MSVC and platform is not Windows, skip this lib
        };

        std::vector<source_file> parse_sources(
            const toml::value& section,
            const std::string& base_path,
            const std::string& key = "sources");

        /// Parses and appends the base path to the library paths.
        /// If the path is already absolute, it will be used as is.
        std::vector<lib_file> parse_libs(
            const toml::value& section,
            const std::string& base_path);

        std::vector<source_file> expand_glob_sources(
            const std::vector<source_file>& input_sources);

        template <typename Derived>
        struct BaseFields {
            std::vector<source_file> sources;
            std::vector<module_file> modules;
            std::vector<lib_file> libs;
            std::unordered_set<std::string> include, defines, undefines;
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
                LOAD_TOML_SOURCES(modules, modules, "modules");
                LOAD_TOML_SOURCES(sources, sources, "sources");

                if constexpr (Derived::enable_include) {
                    auto raw_includes = toml::try_find_or<std::unordered_set<std::string>>(v, "include", {});
                    for (const auto& inc : raw_includes)
                        include.insert(util::file_system::to_linux_path((std::filesystem::path(base_path) / inc).lexically_normal().string()));
                }

                LOAD_TOML_SET_FIELD(defines, defines);
                LOAD_TOML_SET_FIELD(undefines, undefines);
                LOAD_TOML_SET_FIELD(cflags, cflags);
                LOAD_TOML_SET_FIELD(cxxflags, cxxflags);
                LOAD_TOML_SET_FIELD(aflags, aflags);
                LOAD_TOML_SET_FIELD(lflags, lflags);

                LOAD_TOML_LIBS(libs, libs);

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
                SERIALIZE_GLOB_SOURCES(modules);
                SERIALIZE_GLOB_SOURCES(sources);

                SERIALIZE_SOURCES(libs);

                MAYBE_SET_FIELD(include, include);
                MAYBE_SET_FIELD(defines, defines);
                MAYBE_SET_FIELD(undefines, undefines);
                MAYBE_SET_FIELD(cflags, cflags);
                MAYBE_SET_FIELD(cxxflags, cxxflags);
                MAYBE_SET_FIELD(aflags, aflags);
                MAYBE_SET_FIELD(lflags, lflags);

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
        };

        struct CompilerConfig : BaseFields<CompilerConfig> { };

        struct PlatformConfig : BaseFields<PlatformConfig> { };

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

        struct Settings {
            // CXX_Standard cxx_standard;
            // C_Standard c_standard;
            OptimizationLevel optimization_level;
            bool lto = false;
            bool debug = false;
            bool rpath = false;
            bool debug_assertions = false; // -DNDEBUG
        };

        void load(Settings& settings, const toml::value& v);

        void serialize(const Settings& settings, toml::value& out);

        struct Sanitizers {
            bool address = false; // ASan
            bool thread = false; // TSan
            bool undefined = false; // UBSan
            bool memory = false; // MSan
            bool leak = false; // LSan
        };

        void load(Sanitizers& sanitizers, const toml::value& v);

        void serialize(const Sanitizers& sanitizers, toml::value& out);

        struct ProfileConfig : BaseConfig<ProfileConfig> {
            std::string name;
            std::vector<std::string> inherits;

            Settings settings;
            Sanitizers sanitizers;

            void load(
                const toml::value& v,
                const std::string& profile_name,
                const std::string& base_path);

            void serialize(toml::value& out) const;
        };
    } // namespace lockgen
} // namespace muuk