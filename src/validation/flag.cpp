#include "muuk.h"
#include "muukvalidator.hpp"

#include <iostream>
#include <string_view>
#include <stdexcept>

namespace muuk {
    // From cabin @ github.com/cabinpkg/cabin w/ modifications
    Result<bool> validate_flag(Compiler compiler_, std::string_view flag) {
        if (flag.empty()) {
            return Err("{} compiler flag must not be empty", compiler_.to_string());
        }

        if (compiler_ == Compiler::MSVC) {
            // MSVC: Flags start with `/`
            if (flag[0] != '/') {
                return Err("{} compiler flag (`{}`) must start with `/`", compiler_.to_string(), flag);
            }
            // Allowed characters: alphanumeric, `/`, `-`, `:`
            for (char c : flag) {
                if (!std::isalnum(c) && c != '/' && c != '-' && c != ':' && c != '+') {
                    return Err("{} compiler flag (`{}`) contains invalid characters", compiler_.to_string(), flag);
                }
            }
        }
        else {
            // GCC/Clang: Flags start with `-`
            if (flag[0] != '-') {
                return Err("{} compiler flag (`{}`) must start with `-`", compiler_.to_string(), flag);
            }
            // Allowed characters: alphanumeric, `-`, `_`, `=`, `+`, `:`, `.`
            for (char c : flag) {
                if (!std::isalnum(c) && c != '-' && c != '_' && c != '=' &&
                    c != '+' && c != ':' && c != '.') {
                    return Err("{} compiler flag (`{}`) contains invalid characters", compiler_.to_string(), flag);
                }
            }
        }

        return true;
    }
}