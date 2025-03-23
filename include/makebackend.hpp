#pragma once
#ifndef MAKE_BACKEND_HPP
#define MAKE_BACKEND_HPP

#include <filesystem>
#include <memory>
#include <sstream>

#include "buildbackend.hpp"
#include "buildconfig.h"
#include "buildmanager.h"
#include "buildparser.hpp"
#include "buildtargets.h"

namespace fs = std::filesystem;

class MakeBackend : public BuildBackend {
private:
    std::shared_ptr<BuildManager> build_manager;
    std::shared_ptr<BuildParser> build_parser;
    std::string makefile;
    fs::path build_dir_;

public:
    MakeBackend(
        muuk::Compiler compiler,
        const std::string& archiver,
        const std::string& linker,
        const std::string& lockfile_path = MUUK_CACHE_FILE);

    void generate_build_file(
        const std::string& target_build,
        const std::string& profile) override;

private:
    std::string generate_rule(const CompilationTarget& target);
    std::string generate_rule(const ArchiveTarget& target);
    std::string generate_rule(const LinkTarget& target);

    void generate_build_rules(std::ostringstream& out, const std::string& target_build);
    void write_header(std::ostringstream& out, std::string profile);
};

#endif // MAKE_BACKEND_HPP
