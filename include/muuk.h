#pragma once
#ifndef MUUK_H
#define MUUK_H

#include "rustify.hpp"
#include "muukfiler.h"

#include <tl/expected.hpp>
#include <string>
#include <vector>

namespace muuk {
    namespace compiler {
        enum class Compiler {
            GCC,
            Clang,
            MSVC
        };

        std::string to_string(Compiler compiler);
        Compiler from_string(const std::string& compiler_str);
    }

    Result<void> init_project();
    tl::expected<void, std::string> qinit_library(
        const std::string& author,
        const std::string& repo
    );

    // flag handler
    std::string normalize_flag(const std::string& flag, const compiler::Compiler compiler);
    std::string normalize_flags(const std::vector<std::string>& flags, const compiler::Compiler compiler);
    void normalize_flags_inplace(std::vector<std::string>& flags, const compiler::Compiler compiler);

    // void clean(MuukFiler& config_manager);
    // void run_script(
    //     const MuukFiler& config_manager,
    //     const std::string& script,
    //     const std::vector<std::string>& args
    // );
}

#endif // MUUK_H