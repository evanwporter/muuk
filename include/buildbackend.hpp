#pragma once
#ifndef BUILD_BACKEND_H
#define BUILD_BACKEND_H

#include <memory>
#include <vector>
#include <fstream>
// #include "buildtarget.h"  // Include the base class for BuildTarget
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

#endif  // BUILD_BACKEND_H