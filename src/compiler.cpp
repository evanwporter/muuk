#include "compiler.hpp"
#include "rustify.hpp"
#include <stdexcept>

namespace muuk {

    const Compiler Compiler::GCC(Compiler::Type::GCC);
    const Compiler Compiler::Clang(Compiler::Type::Clang);
    const Compiler Compiler::MSVC(Compiler::Type::MSVC);

    Result<Compiler> Compiler::from_string(const std::string& compilerStr) {
        static const std::unordered_map<std::string, Type> compilerMap = {
            { "g++", Type::GCC }, { "gcc", Type::GCC },
            { "clang++", Type::Clang }, { "clang", Type::Clang },
            { "cl", Type::MSVC }, { "msvc", Type::MSVC }
        };

        auto it = compilerMap.find(compilerStr);
        if (it != compilerMap.end()) {
            return Compiler(it->second);
        }

        return Err("Unknown compiler: {}. Acceptable compilers are `gcc`, `clang` and `msvc`", compilerStr);
    }

    std::string Compiler::to_string(Type type) {
        switch (type) {
        case Type::GCC: return "g++";
        case Type::Clang: return "clang++";
        case Type::MSVC: return "cl";
        default: return "Unknown";
        }
    }

    std::string Compiler::to_string() const {
        switch (type_) {
        case Type::GCC: return "g++";
        case Type::Clang: return "clang++";
        case Type::MSVC: return "cl";
        }
    }

    // Detect archiver based on compiler
    std::string Compiler::detect_archiver() const {
        switch (type_) {
        case Type::MSVC: return "lib";
        case Type::Clang: return "llvm-ar";
        case Type::GCC: return "ar";
        }
    }

    // Detect linker based on compiler
    std::string Compiler::detect_linker() const {
        switch (type_) {
        case Type::MSVC: return "link";
        case Type::Clang:
        case Type::GCC: return to_string(); // Compiler acts as linker
        }
    }

    const Edition Edition::Cpp98(Edition::Year::Cpp98);
    const Edition Edition::Cpp03(Edition::Year::Cpp03);
    const Edition Edition::Cpp11(Edition::Year::Cpp11);
    const Edition Edition::Cpp14(Edition::Year::Cpp14);
    const Edition Edition::Cpp17(Edition::Year::Cpp17);
    const Edition Edition::Cpp20(Edition::Year::Cpp20);
    const Edition Edition::Cpp23(Edition::Year::Cpp23);
    const Edition Edition::Cpp26(Edition::Year::Cpp26);
    const Edition Edition::Unknown(Edition::Year::Unknown);

    std::optional<Edition::Year> Edition::from_string(const std::string& str) {
        static const std::unordered_map<std::string, Year> editionMap = {
            { "98", Year::Cpp98 },
            { "03", Year::Cpp03 },
            { "0x", Year::Cpp11 }, { "11", Year::Cpp11 },
            { "1y", Year::Cpp14 }, { "14", Year::Cpp14 },
            { "1z", Year::Cpp17 }, { "17", Year::Cpp17 },
            { "2a", Year::Cpp20 }, { "20", Year::Cpp20 },
            { "2b", Year::Cpp23 }, { "23", Year::Cpp23 },
            { "2c", Year::Cpp26 }
        };

        if (str.size() < 2) {
            return std::nullopt; // Not enough characters to determine edition
        }

        // Extract the last two characters
        std::string lastTwo = str.substr(str.size() - 2);

        auto it = editionMap.find(lastTwo);
        if (it != editionMap.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::string Edition::to_string() const {
        switch (year_) {
        case Year::Cpp98: return "C++98";
        case Year::Cpp03: return "C++03";
        case Year::Cpp11: return "C++11";
        case Year::Cpp14: return "C++14";
        case Year::Cpp17: return "C++17";
        case Year::Cpp20: return "C++20";
        case Year::Cpp23: return "C++23";
        case Year::Cpp26: return "C++26";
        default: return "Unknown";
        }
    }

    // Convert an Edition to a compiler-specific flag
    std::string Edition::to_flag(const Compiler& compiler) const {
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
            return msvcFlags.count(year_) ? msvcFlags.at(year_) : "/std:c++latest";
        }
        else {
            return gccClangFlags.count(year_) ? gccClangFlags.at(year_) : "-std=c++2b"; // Default for GCC/Clang
        }
    }

    namespace compiler {
        std::string to_string(Compiler compiler) {
            switch (compiler) {
            case Compiler::GCC: return "g++";
            case Compiler::Clang: return "clang++";
            case Compiler::MSVC: return "cl";
            }
            throw std::invalid_argument("Invalid compiler type");
        }

        Compiler from_string(const std::string& compiler_str) {
            if (compiler_str == "g++" || compiler_str == "gcc") return Compiler::GCC;
            if (compiler_str == "clang++" || compiler_str == "clang") return Compiler::Clang;
            if (compiler_str == "cl" || compiler_str == "msvc") return Compiler::MSVC;
            throw std::invalid_argument("Unsupported compiler: " + compiler_str);
        }
    }
} // namespace muuk

