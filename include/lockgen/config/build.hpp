#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"
#include "lockgen/config/package.hpp"

namespace muuk {
    namespace lockgen {

        struct Build : BaseConfig<Build> {
            std::unordered_set<std::string> profiles;
            std::unordered_set<std::shared_ptr<Dependency>> all_dependencies_array;

            static constexpr bool enable_compilers = false;
            static constexpr bool enable_platforms = false;

            muuk::BuildLinkType link_type = muuk::BuildLinkType::EXECUTABLE;

            void merge(const Package& package);
            Result<void> serialize(toml::value& out) const;
            void load(const toml::value& v, const std::string& base_path);
        };

    } // namespace lockgen
} // namespace muuk