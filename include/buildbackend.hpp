#pragma once
#ifndef BUILD_BACKEND_H
#define BUILD_BACKEND_H

#include <filesystem>
#include <memory>

#include <nlohmann/json.hpp>

#include "buildmanager.h"
#include "buildtargets.h"
#include "compiler.hpp"

class BuildBackend {
protected:
    muuk::Compiler compiler_;
    const std::string archiver_;
    const std::string linker_;

public:
    virtual ~BuildBackend() = default;

    BuildBackend(
        muuk::Compiler compiler,
        std::string archiver,
        std::string linker) :
        compiler_(compiler),
        archiver_(std::move(archiver)),
        linker_(std::move(linker)) {
    }

    virtual void generate_build_file(
        const std::string& target_build,
        const std::string& profile)
        = 0;
};

class NinjaBackend : public BuildBackend {
private:
    std::unique_ptr<BuildManager> build_manager;
    std::string ninja_filename;
    std::filesystem::path build_dir_;

public:
    NinjaBackend(
        muuk::Compiler compiler,
        const std::string& archiver,
        const std::string& linker);

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

class CompileCommandsBackend : public BuildBackend {
private:
    std::unique_ptr<BuildManager> build_manager;
    std::filesystem::path build_dir_;

public:
    CompileCommandsBackend(
        muuk::Compiler compiler,
        const std::string& archiver,
        const std::string& linker);

    void generate_build_file(
        const std::string& target_build,
        const std::string& profile) override;

private:
    nlohmann::json generate_compile_commands(const std::string& profile_cflags);
};

#endif // BUILD_BACKEND_H