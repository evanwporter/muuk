#pragma once
#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <toml.hpp>

#include "buildmanager.h"
#include "compiler.hpp"

class BuildParser {
private:
    std::shared_ptr<BuildManager> build_manager;
    const toml::value muuk_file;
    const std::filesystem::path& build_dir;
    muuk::Compiler compiler;
    std::string profile_;

public:
    BuildParser(
        std::shared_ptr<BuildManager> manager,
        muuk::Compiler compiler,
        const std::filesystem::path& build_dir,
        std::string profile);

    void parse();

    std::pair<std::string, std::string> extract_profile_flags(const std::string& profile);

private:
    void parse_compilation_targets();
    void parse_libraries();
    void parse_executables();

    void parse_compilation_unit(
        const toml::array& unit_array,
        const std::string& name,
        const std::filesystem::path& pkg_dir,
        const std::vector<std::string>& base_cflags,
        const std::vector<std::string>& platform_cflags,
        const std::vector<std::string>& compiler_cflags,
        const std::vector<std::string>& iflags);

    // Extract platform-specific FLAGS
    std::vector<std::string> extract_platform_flags(const toml::table& package_table);

    // Extract compiler-specific FLAGS */
    std::vector<std::string> extract_compiler_flags(const toml::table& package_table);
};

#endif