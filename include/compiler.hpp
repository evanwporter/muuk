#pragma once
#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "rustify.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

namespace muuk {

    class Compiler {
    public:
        enum class Type {
            GCC,
            Clang,
            MSVC,
        };

        static const Compiler GCC;
        static const Compiler Clang;
        static const Compiler MSVC;

        explicit Compiler(Type type) : type_(type) {}

        static Result<Compiler> from_string(const std::string& compilerStr);
        static std::string to_string(Type type);
        std::string to_string() const;

        std::string detect_archiver() const;
        std::string detect_linker() const;

        Type getType() const { return type_; }

        bool operator==(const Compiler& other) const { return type_ == other.type_; }
        bool operator!=(const Compiler& other) const { return type_ != other.type_; }

    private:
        Type type_;
    };

    namespace compiler {
        enum class Compiler {
            GCC,
            Clang,
            MSVC
        };

        std::string to_string(Compiler compiler);
        Compiler from_string(const std::string& compiler_str);
    } // namespace compiler

    class Edition {
    public:
        enum class Year {
            Cpp98 = 1998,
            Cpp03 = 2003,
            Cpp11 = 2011,
            Cpp14 = 2014,
            Cpp17 = 2017,
            Cpp20 = 2020,
            Cpp23 = 2023,
            Cpp26 = 2026,
            Unknown = 0
        };

        explicit Edition(Year year) : year_(year) {}

        static std::optional<Year> from_string(const std::string& str);
        std::string to_string() const;
        std::string to_flag(const Compiler& compiler) const;

        static const Edition Cpp98;
        static const Edition Cpp03;
        static const Edition Cpp11;
        static const Edition Cpp14;
        static const Edition Cpp17;
        static const Edition Cpp20;
        static const Edition Cpp23;
        static const Edition Cpp26;
        static const Edition Unknown;

    private:
        Year year_;

    };
} // namespace muuk

#endif