#include <string_view>

#include "compiler.hpp"
#include "muukvalidator.hpp"
#include "rustify.hpp"

namespace muuk {
    // From cabin @ github.com/cabinpkg/cabin w/ modifications
    // Credit to Ken Matsui @ github.com/ken-matsui
    Result<bool> validate_flag(Compiler compiler_, std::string_view flag) {
        if (flag.empty()) {
            return Err("{} compiler flag must not be empty", compiler_.to_string());
        }

        if (compiler_ == Compiler::MSVC) {
            // MSVC: Flags start with `/` or `-`
            if (flag[0] != '/' && flag[0] != '-') {
                return Err("{} compiler flag (`{}`) must start with `/`", compiler_.to_string(), flag);
            }
            // Allowed characters: alphanumeric, `/`, `-`, `:`
            for (char c : flag) {
                if (!std::isalnum(c) && c != '/' && c != '-' && c != ':' && c != '+' && c != '_' && c != '.' && c != '=') {
                    return Err("{} compiler flag (`{}`) contains invalid characters", compiler_.to_string(), flag);
                }
            }
        } else {
            // GCC/Clang: Flags start with `-`
            if (flag[0] != '-') {
                return Err("{} compiler flag (`{}`) must start with `-`", compiler_.to_string(), flag);
            }
            // Allowed characters: alphanumeric, `-`, `_`, `=`, `+`, `:`, `.`
            for (char c : flag) {
                if (!std::isalnum(c) && c != '-' && c != '_' && c != '=' && c != '+' && c != ':' && c != '.' && c != '/') {
                    return Err("{} compiler flag (`{}`) contains invalid characters", compiler_.to_string(), flag);
                }
            }
        }

        return true;
    }
}