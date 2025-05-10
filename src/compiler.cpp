#include <string>
#include <unordered_map>

#include "compiler.hpp"
#include "rustify.hpp"

namespace muuk {

    const Compiler Compiler::GCC(Compiler::Type::GCC);
    const Compiler Compiler::Clang(Compiler::Type::Clang);
    const Compiler Compiler::MSVC(Compiler::Type::MSVC);

    Result<Compiler> Compiler::from_string(const std::string& compilerStr) {
        static const std::unordered_map<std::string, Type> compiler_map = {
            { "g++", Type::GCC },
            { "gcc", Type::GCC },
            { "clang++", Type::Clang },
            { "clang", Type::Clang },
            { "cl", Type::MSVC },
            { "msvc", Type::MSVC }
        };

        auto it = compiler_map.find(compilerStr);
        if (it != compiler_map.end())
            return Compiler(it->second);

        return Err("Unknown compiler: {}. Acceptable compilers are `gcc`, `clang` and `msvc`", compilerStr);
    }

    std::string Compiler::to_string(Type type) {
        switch (type) {
        case Type::GCC:
            return "g++";
        case Type::Clang:
            return "clang++";
        case Type::MSVC:
            return "cl";
        default:
            return "Unknown";
        }
    }

    std::string Compiler::to_string() const {
        switch (type_) {
        case Type::GCC:
            return "g++";
        case Type::Clang:
            return "clang++";
        case Type::MSVC:
            return "cl";
        }
        return "clang++";
    }

    /// Detect archiver based on compiler
    std::string Compiler::detect_archiver() const {
        switch (type_) {
        case Type::MSVC:
            return "lib";
        case Type::Clang:
#ifdef WIN32
            return "llvm-ar";
#else
            return "ar";
#endif
        case Type::GCC:
            return "ar";
        }
        return "llvm-ar";
    }

    /// Detect linker based on compiler
    std::string Compiler::detect_linker() const {
        switch (type_) {
        case Type::MSVC:
            return "link";
        case Type::Clang:
        case Type::GCC:
            return to_string(); // Compiler acts as linker
        }
        return to_string();
    }

    const CXX_Standard CXX_Standard::Cpp98(CXX_Standard::Year::Cpp98);
    const CXX_Standard CXX_Standard::Cpp03(CXX_Standard::Year::Cpp03);
    const CXX_Standard CXX_Standard::Cpp11(CXX_Standard::Year::Cpp11);
    const CXX_Standard CXX_Standard::Cpp14(CXX_Standard::Year::Cpp14);
    const CXX_Standard CXX_Standard::Cpp17(CXX_Standard::Year::Cpp17);
    const CXX_Standard CXX_Standard::Cpp20(CXX_Standard::Year::Cpp20);
    const CXX_Standard CXX_Standard::Cpp23(CXX_Standard::Year::Cpp23);
    const CXX_Standard CXX_Standard::Cpp26(CXX_Standard::Year::Cpp26);
    const CXX_Standard CXX_Standard::Unknown(CXX_Standard::Year::Unknown);

    CXX_Standard CXX_Standard::from_string(const std::string& str) {
        static const std::unordered_map<std::string, Year> editionMap = {
            { "98", Year::Cpp98 },
            { "03", Year::Cpp03 },
            { "0x", Year::Cpp11 },
            { "11", Year::Cpp11 },
            { "1y", Year::Cpp14 },
            { "14", Year::Cpp14 },
            { "1z", Year::Cpp17 },
            { "17", Year::Cpp17 },
            { "2a", Year::Cpp20 },
            { "20", Year::Cpp20 },
            { "2b", Year::Cpp23 },
            { "23", Year::Cpp23 },
            { "2c", Year::Cpp26 }
        };

        if (str.size() < 2) {
            return CXX_Standard::Unknown; // Not enough characters to determine edition
        }

        // Extract the last two characters
        std::string lastTwo = str.substr(str.size() - 2);

        auto it = editionMap.find(lastTwo);
        if (it != editionMap.end()) {
            return CXX_Standard(it->second);
        }
        return CXX_Standard::Unknown;
    }

    std::string CXX_Standard::to_string() const {
        switch (year_) {
        case Year::Cpp98:
            return "C++98";
        case Year::Cpp03:
            return "C++03";
        case Year::Cpp11:
            return "C++11";
        case Year::Cpp14:
            return "C++14";
        case Year::Cpp17:
            return "C++17";
        case Year::Cpp20:
            return "C++20";
        case Year::Cpp23:
            return "C++23";
        case Year::Cpp26:
            return "C++26";
        default:
            return "Unknown";
        }
    }

    // Convert an CXX_Standard to a compiler-specific flag
    std::string CXX_Standard::to_flag(const Compiler& compiler) const {
        static const std::unordered_map<Year, std::string> gccClangFlags = {
            { Year::Cpp98, "-std=c++98" },
            { Year::Cpp03, "-std=c++03" },
            { Year::Cpp11, "-std=c++11" },
            { Year::Cpp14, "-std=c++14" },
            { Year::Cpp17, "-std=c++17" },
            { Year::Cpp20, "-std=c++20" },
            { Year::Cpp23, "-std=c++23" },
            { Year::Cpp26, "-std=c++26" }
        };

        static const std::unordered_map<Year, std::string> msvcFlags = {
            { Year::Cpp98, "/std:c++98" },
            { Year::Cpp03, "/std:c++03" },
            { Year::Cpp11, "/std:c++11" },
            { Year::Cpp14, "/std:c++14" },
            { Year::Cpp17, "/std:c++17" },
            { Year::Cpp20, "/std:c++20" },
            { Year::Cpp23, "/std:c++23" },
            { Year::Cpp26, "/std:c++latest" } // MSVC doesn't have C++26 yet
        };

        if (compiler == Compiler::MSVC) {
            return msvcFlags.count(year_)
                ? msvcFlags.at(year_)
                : "/std:c++latest"; // Default for MSVC
        } else {
            return gccClangFlags.count(year_)
                ? gccClangFlags.at(year_)
                : "-std=c++20"; // Default for GCC/Clang
        }
    }

    std::string CXX_Standard::to_flag() const {
        return to_flag(Compiler::GCC); // Default to GCC
    }

    std::string to_string(BuildLinkType type) {
        switch (type) {
        case BuildLinkType::EXECUTABLE:
            return "binary";
        case BuildLinkType::STATIC:
            return "static";
        case BuildLinkType::SHARED:
            return "shared";
        default:
            return "";
        }
    }

    BuildLinkType build_link_from_string(const std::string& str) {
        if (str == "binary")
            return BuildLinkType::EXECUTABLE;
        else if (str == "static")
            return BuildLinkType::STATIC;
        else if (str == "shared")
            return BuildLinkType::SHARED;
        else
            return BuildLinkType::EXECUTABLE; // Default
    }

    std::string to_string(LinkType type) {
        switch (type) {
        case LinkType::STATIC:
            return "static";
        case LinkType::SHARED:
            return "shared";
        case LinkType::NO_LINK:
            return "no_link";
        default:
            return "";
        }
    }

} // namespace muuk
