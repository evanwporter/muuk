#pragma once
#include <string>
#include <unordered_set>

#ifdef _WIN32
#ifdef _M_X64 // Ensure we are building for 64-bit
constexpr const char* COMPILER = "cl /nologo /EHsc /std:c++20 /MT /arch:AVX2";
constexpr const char* ARCHIVER = "lib";
constexpr const char* LINKER = "link /MACHINE:X64";
#else
constexpr const char* COMPILER = "cl /nologo /EHsc /std:c++20 /MT /arch:SSE2";
constexpr const char* ARCHIVER = "lib";
constexpr const char* LINKER = "link /MACHINE:X86";
#endif
constexpr const char* OBJ_EXT = ".obj";
constexpr const char* LIB_EXT = ".lib";
constexpr const char* EXE_EXT = ".exe";

#else // Linux/macOS
#ifdef __x86_64__
constexpr const char* COMPILER = "g++ -std=c++20 -m64 -march=native -O2";
constexpr const char* ARCHIVER = "ar rcs";
constexpr const char* LINKER = "g++ -m64";
#else
constexpr const char* COMPILER = "g++ -std=c++20 -m32 -march=i686 -O2";
constexpr const char* ARCHIVER = "ar rcs";
constexpr const char* LINKER = "g++ -m32";
#endif
constexpr const char* OBJ_EXT = ".o";
constexpr const char* LIB_EXT = ".a";
constexpr const char* EXE_EXT = "";
#endif

constexpr const char* DEPENDENCY_FOLDER = "deps";
constexpr std::string_view BUILD_FOLDER = "build";

const std::string MUUK_CACHE_FILE = "build/muuk.lock.toml";
const std::string MUUK_TOML_FILE = "muuk.toml";

// Credit to Ken Matsui @ Cabin for this snippet
// https://github.com/cabinpkg/cabin/blob/4c1eb8549ff3b0532ae1bc78ed38f07436450e3e/src/BuildConfig.hpp#L22C1-L27C3
inline const std::unordered_set<std::string> SOURCE_FILE_EXTS {
    ".c", ".c++", ".cc", ".cpp", ".cxx"
};
inline const std::unordered_set<std::string> HEADER_FILE_EXTS {
    ".h", ".h++", ".hh", ".hpp", ".hxx"
};
