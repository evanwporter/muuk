#pragma once

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


    void init_project();

    // flag handler
    std::string normalize_flag(const std::string& flag, const compiler::Compiler compiler);
    std::string normalize_flags(const std::vector<std::string>& flags, const compiler::Compiler compiler);
}