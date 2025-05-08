#pragma once
#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include <filesystem>
#include <string>

#include <toml.hpp>

#include "build/manager.hpp"
#include "build/targets.hpp"
#include "compiler.hpp"

namespace muuk {
    namespace build {
        std::tuple<std::string, std::string, std::string> get_profile_flag_strings(
            const BuildManager& manager,
            const std::string& profile);

        Result<void> parse(
            BuildManager& build_manager,
            const muuk::Compiler compiler,
            const std::filesystem::path& build_dir,
            const std::string& profile);

        void parse_compilation_targets(
            BuildManager& build_manager,
            const muuk::Compiler compiler,
            const std::filesystem::path& build_dir,
            const std::string& profile,
            const toml::value& muuk_file);

        void parse_libraries(
            BuildManager& build_manager,
            const muuk::Compiler compiler,
            const std::filesystem::path& build_dir,
            const toml::value& muuk_file);

        void parse_executables(
            BuildManager& build_manager,
            const muuk::Compiler compiler,
            const std::filesystem::path& build_dir,
            const std::string& profile,
            const toml::value& muuk_file);

        void parse_compilation_unit(
            BuildManager& build_manager,
            const toml::array& unit_array,
            const CompilationUnitType compilation_unit_type,
            const std::filesystem::path& pkg_dir,
            const CompilationFlags compilation_flags);

    } // namespace build
} // namespace muuk

#endif