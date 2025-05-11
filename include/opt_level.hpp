#pragma once
#ifndef MUUK_OPT_LEVEL_HPP
#define MUUK_OPT_LEVEL_HPP

#include <string>

#include <compiler.hpp>
#include <logger.hpp>

#define OPTIMIZATION_LEVELS(X)           \
    X(O0, '0', "-O0", "-O0", "/Od")      \
    X(O1, '1', "-O1", "-O1", "/O1")      \
    X(O2, '2', "-O2", "-O2", "/O2")      \
    X(O3, '3', "-O3", "-O3", "/Ox /Ob2") \
    X(Os, 's', "-Os", "-Os", "/Os")      \
    X(Oz, 'z', "-Os", "-Oz", "/Os")
//              CLANG,  GCC,   MSVC

#define X_ENUM(name, chr, gcc, clang, msvc) name,
enum class OptimizationLevel {
    OPTIMIZATION_LEVELS(X_ENUM)
};

namespace muuk {
    inline OptimizationLevel opt_lvl_from_string(const std::string& str) {
        if (str.empty())
            return OptimizationLevel::O1;

        char c;
        if ((str.size() == 2 || str.size() == 1) && (str[0] == 'O' || str[0] == 'o')) {
            c = str.size() == 2 ? str[1] : '\0';
        } else if (str.size() == 1) {
            c = str[0];
        } else {
            muuk::logger::warn("Invalid optimization level string: '{}'. Defaulting to O1.", str);
            return OptimizationLevel::O1;
        }

#define X_FROM_CHAR(name, chr, gcc, clang, msvc) \
    if (c == chr)                                \
        return OptimizationLevel::name;
        OPTIMIZATION_LEVELS(X_FROM_CHAR)
#undef X_FROM_CHAR

        return OptimizationLevel::O1; // Default optimization level
    }

    inline std::string to_flag(OptimizationLevel level, Compiler::Type compiler) {
        switch (level) {
#define X_TO_FLAG(name, chr, gcc, clang, msvc) \
    case OptimizationLevel::name:              \
        switch (compiler) {                    \
        case Compiler::Type::GCC:              \
            return gcc;                        \
        case Compiler::Type::Clang:            \
            return clang;                      \
        case Compiler::Type::MSVC:             \
            return msvc;                       \
        default:                               \
            return "";                         \
        }
            OPTIMIZATION_LEVELS(X_TO_FLAG)
#undef X_TO_FLAG
        }

        return "";
    }

    inline std::string to_string(OptimizationLevel level) {
        switch (level) {
#define X_TO_CHAR(name, chr, gcc, clang, msvc) \
    case OptimizationLevel::name:              \
        return std::string(1, chr);
            OPTIMIZATION_LEVELS(X_TO_CHAR)
#undef X_TO_CHAR
        }
        return "1"; // Default string
    }
}

#undef OPTIMIZATION_LEVELS

#endif // MUUK_OPT_LEVEL_HPP
