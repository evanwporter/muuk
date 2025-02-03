#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H

#ifdef _WIN32
constexpr const char* COMPILER = "cl";
constexpr const char* ARCHIVER = "lib";
constexpr const char* LINKER = "cl";
constexpr const char* OBJ_EXT = ".obj";
constexpr const char* LIB_EXT = ".lib";
constexpr const char* EXE_EXT = ".exe";
#else
constexpr const char* COMPILER = "g++";  // Change to "clang++" if needed
constexpr const char* ARCHIVER = "ar rcs";
constexpr const char* LINKER = "g++";    // Change to "clang++" if needed
constexpr const char* OBJ_EXT = ".o";
constexpr const char* LIB_EXT = ".a";
constexpr const char* EXE_EXT = "";
#endif

#endif // BUILD_CONFIG_H
