#pragma once
#ifndef BUILD_BACKEND_H
#define BUILD_BACKEND_H

#include <memory>
#include <vector>
#include <fstream>
#include <filesystem>
#include "buildtargets.h" 
#include "buildmanager.h"
#include "buildparser.hpp"
#include "buildtargets.h"
#include "muukfiler.h"

class BuildBackend {
protected:
    muuk::compiler::Compiler compiler_;
    const std::string archiver_;
    const std::string linker_;
    std::shared_ptr<MuukFiler> muuk_filer;

public:
    virtual ~BuildBackend() = default;

    BuildBackend(
        muuk::compiler::Compiler compiler,
        std::string archiver, std::string linker,
        const std::string& lockfile_path
    ) :
        compiler_(compiler),
        archiver_(std::move(archiver)),
        linker_(std::move(linker)),
        muuk_filer(std::make_shared<MuukFiler>(lockfile_path))
    {
    }

    virtual void generate_build_file(
        const std::string& target_build,
        const std::string& profile
    ) = 0;
};

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

#endif  // BUILD_BACKEND_H