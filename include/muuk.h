#pragma once
#ifndef MUUK_H
#define MUUK_H

#include <string>
#include <vector>

#include <tl/expected.hpp>

#include "compiler.hpp"
#include "muukfiler.h"
#include "rustify.hpp"

// { Dependency { Versioning { T }}}
template <typename T>
using DependencyVersionMap = std::unordered_map<std::string, std::unordered_map<std::string, T>>;

namespace muuk {
    Result<void> init_project();
    tl::expected<void, std::string> qinit_library(
        const std::string& author,
        const std::string& repo,
        const std::string& version);

    // flag handler
    std::string normalize_flag(const std::string& flag, const Compiler compiler);
    std::string normalize_flags(const std::vector<std::string>& flags, const Compiler compiler);
    void normalize_flags_inplace(std::vector<std::string>& flags, const Compiler compiler);

    // void clean(MuukFiler& config_manager);
    // void run_script(
    //     const MuukFiler& config_manager,
    //     const std::string& script,
    //     const std::vector<std::string>& args
    // );
}

#endif // MUUK_H