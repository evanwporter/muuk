#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"
#include "lockgen/config/library.hpp"
#include "muuk.hpp"

namespace muuk {
    namespace lockgen {
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

            /// External build settings (cmake, meson, etc.) `[external]`.
            External external_config;

            /// This is chiefly used for the resolve_system_dependency function
            /// to add include paths to the library config.
            void add_include_path(std::string path) {
                library_config.include.insert(path);
            }

            /// This is chiefly used for the resolve_system_dependency function
            /// to add library paths to the library config.
            void add_lib_path(std::string path) {
                library_config.libs.push_back(lib_file(path));
            }

        private:
            static void serialize_dependencies(
                std::ostringstream& toml_stream,
                const DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_,
                const std::string section_name);
        };

    } // namespace lockgen
} // namespace muuk