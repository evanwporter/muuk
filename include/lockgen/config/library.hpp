#pragma once

#include <string>
#include <unordered_set>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"

namespace muuk {
    namespace lockgen {
        struct Library : BaseConfig<Library> {
            std::string name;
            std::string version;
            std::unordered_set<std::string> profiles;

            muuk::LinkType link_type = muuk::LinkType::STATIC;

            static constexpr bool enable_compilers = false;
            static constexpr bool enable_platforms = false;
            static constexpr bool enable_dependencies = false;

            void load(const std::string& name_, const std::string& version_, const std::string& base_path_, const toml::value& v);
            void serialize(toml::value& out, Platforms platforms, Compilers compilers) const;
        };

        struct ExternalOutput {
            std::string path;
            std::string profile;
        };

        struct External {
            std::string name;
            std::string version;
            std::unordered_set<std::string> profiles;

            std::string type;
            std::vector<std::string> args;
            std::vector<ExternalOutput> outputs;

            std::string source_file;

            std::string path;

            void load(
                const std::string& name_,
                const std::string& version_,
                const std::string& base_path_,
                const toml::value& v);

            void serialize(toml::value& out) const;
        };
    } // namespace lockgen
} // namespace muuk