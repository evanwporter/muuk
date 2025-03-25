#pragma once
#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include <filesystem>
#include <string>
#include <vector>

#include <toml.hpp>

#include "buildmanager.h"
#include "compiler.hpp"

std::pair<std::string, std::string> get_profile_flag_strings(
    BuildManager& manager,
    const std::string& profile);

Result<void> parse(
    BuildManager& build_manager,
    muuk::Compiler compiler,
    const std::filesystem::path& build_dir,
    const std::string& profile);

void parse_compilation_targets(
    BuildManager& build_manager,
    muuk::Compiler compiler,
    const std::filesystem::path& build_dir,
    const std::string& profile,
    const toml::value& muuk_file);

void parse_libraries(
    BuildManager& build_manager,
    muuk::Compiler compiler,
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
    const muuk::Compiler compiler,
    const toml::array& unit_array,
    const std::string& name,
    const std::filesystem::path& pkg_dir,
    const std::vector<std::string>& base_cflags,
    const std::vector<std::string>& platform_cflags,
    const std::vector<std::string>& compiler_cflags,
    const std::vector<std::string>& iflags);

std::vector<std::string> extract_platform_flags(
    const toml::table& package_table,
    muuk::Compiler compiler);

std::vector<std::string> extract_compiler_flags(
    const toml::table& package_table,
    muuk::Compiler compiler);

#endif