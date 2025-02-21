#pragma once
#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include "./buildmanager.h"
#include "./muukfiler.h"
#include "logger.h"
#include "util.h"
#include "muuk.h"
#include "./buildconfig.h"

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

class BuildParser {
private:
    std::shared_ptr<BuildManager> build_manager;
    std::shared_ptr<MuukFiler> muuk_filer;
    toml::table config_;
    fs::path build_dir;
    muuk::compiler::Compiler compiler;
    std::string profile_;

public:
    BuildParser(
        std::shared_ptr<BuildManager> manager,
        std::shared_ptr<MuukFiler> muuk_filer,
        muuk::compiler::Compiler compiler,
        fs::path build_dir,
        const std::string profile
    );

    void parse();

    std::pair<std::string, std::string> extract_profile_flags(const std::string& profile);

private:
    void parse_compilation_targets();
    void parse_libraries();
    void parse_executables();

    std::vector<std::string> extract_flags(
        const toml::table& table,
        const std::string& key,
        const std::string& prefix = ""
    );

    /** Extract platform-specific CFLAGS */
    std::vector<std::string> extract_platform_flags(const toml::table& package_table);

    /** Extract compiler-specific CFLAGS */
    std::vector<std::string> extract_compiler_flags(const toml::table& package_table);

};

#endif