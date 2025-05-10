#pragma once
#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <optional>
#include <string>

#include "rustify.hpp"

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

        explicit Compiler(Type type) :
            type_(type) { }

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

    // Inspired by Ken Matsui @ Cabin
    // https://github.com/cabinpkg/cabin/blob/1031568a40abb4d9e915bb1c537d62a502603d1c/src/Manifest.hpp#L25-L60
    class CXX_Standard {
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

        CXX_Standard() :
            year_(Year::Unknown) { }

        explicit CXX_Standard(Year year) :
            year_(year) { }

        static CXX_Standard from_string(const std::string& str);
        std::string to_string() const;
        std::string to_flag(const Compiler& compiler) const;
        std::string to_flag() const;

        // Comparison operators
        bool operator==(const CXX_Standard& other) const {
            return year_ == other.year_;
        }

        bool operator!=(const CXX_Standard& other) const {
            return !(*this == other);
        }

        bool operator<(const CXX_Standard& other) const {
            return static_cast<int>(year_) < static_cast<int>(other.year_);
        }

        bool operator<=(const CXX_Standard& other) const {
            return *this < other || *this == other;
        }

        bool operator>(const CXX_Standard& other) const {
            return !(*this <= other);
        }

        bool operator>=(const CXX_Standard& other) const {
            return !(*this < other);
        }

        static const CXX_Standard Cpp98;
        static const CXX_Standard Cpp03;
        static const CXX_Standard Cpp11;
        static const CXX_Standard Cpp14;
        static const CXX_Standard Cpp17;
        static const CXX_Standard Cpp20;
        static const CXX_Standard Cpp23;
        static const CXX_Standard Cpp26;
        static const CXX_Standard Unknown;

    private:
        Year year_;
    };

    class C_Standard {
    public:
        enum class Year {
            C89 = 1989,
            C99 = 1999,
            C11 = 2011,
            C17 = 2017,
            C23 = 2023,
            Unknown = 0
        };

        C_Standard() :
            year_(Year::Unknown) { }

        explicit C_Standard(Year year) :
            year_(year) { }

        static std::optional<C_Standard> from_string(const std::string& str);
        std::string to_string() const;
        std::string to_flag(const Compiler& compiler) const;
        std::string to_flag() const;

        static const C_Standard C89;
        static const C_Standard C99;
        static const C_Standard C11;
        static const C_Standard C17;
        static const C_Standard C23;
        static const C_Standard Unknown;

    private:
        Year year_;
    };

    enum class LinkType {
        STATIC,
        SHARED,
        NO_LINK
    };

    enum class BuildLinkType {
        EXECUTABLE,
        STATIC,
        SHARED
    };

    std::string to_string(BuildLinkType type);

    BuildLinkType build_link_from_string(const std::string& str);

    std::string to_string(LinkType type);

} // namespace muuk

#endif