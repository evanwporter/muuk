#pragma once
#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "buildmanager.h"
#include "compiler.hpp"
#include "muukfiler.h"

class BuildParser {
private:
    std::shared_ptr<BuildManager> build_manager;
    std::shared_ptr<MuukFiler> muuk_filer;
    toml::table config_;
    const std::filesystem::path& build_dir;
    muuk::Compiler compiler;
    std::string profile_;

public:
    BuildParser(
        std::shared_ptr<BuildManager> manager,
        std::shared_ptr<MuukFiler> muuk_filer,
        muuk::Compiler compiler,
        const std::filesystem::path& build_dir,
        std::string profile);

    void parse();

    std::pair<std::string, std::string> extract_profile_flags(const std::string& profile);

private:
    void parse_compilation_targets();
    void parse_libraries();
    void parse_executables();

    std::vector<std::string> extract_flags(
        const toml::table& table,
        const std::string& key,
        const std::string& prefix = "");

    /** Extract platform-specific CFLAGS */
    std::vector<std::string> extract_platform_flags(const toml::table& package_table);

    /** Extract compiler-specific CFLAGS */
    std::vector<std::string> extract_compiler_flags(const toml::table& package_table);
};

#endif