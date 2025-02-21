#pragma once
#ifndef NINJA_BACKEND_HPP
#define NINJA_BACKEND_HPP

#include "./buildbackend.hpp"
#include "./buildtargets.h"
#include "./buildparser.hpp"
#include "./buildmanager.h"

#include <fstream>
#include <memory>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

class NinjaBackend : public BuildBackend {
private:
    std::shared_ptr<BuildManager> build_manager;
    std::shared_ptr<BuildParser> build_parser;
    std::string ninja_filename;
    fs::path build_dir_;

public:
    NinjaBackend(
        muuk::compiler::Compiler compiler,
        const std::string& archiver,
        const std::string& linker,
        const std::string& lockfile_path = "muuk.lock.toml"
    );

    void generate_build_file(
        const std::string& target_build,
        const std::string& profile
    ) override;

private:
    std::string generate_rule(const CompilationTarget& target);
    std::string generate_rule(const ArchiveTarget& target);
    std::string generate_rule(const LinkTarget& target);

    void generate_build_rules(std::ostringstream& out, const std::string& target_build);
    void write_header(std::ostringstream& out, std::string profile);
};

#endif // NINJA_BACKEND_HPP
