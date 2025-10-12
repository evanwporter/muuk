#pragma once
#include <toml.hpp>

#include "structs/opt_level.hpp"

namespace muuk {

    enum class WarningLevel {
        Off = 0, // No warnings
        Level1 = 1,
        Level2 = 2,
        Level3 = 3,
        Level4 = 4
    };

    enum class ExceptionHandling {
        Default, // /EHsc (MSVC) or -fexceptions (GCC/Clang)
        None, // Disable exceptions
        Asynchronous // /EHa (MSVC) or -fnon-call-exceptions (GCC/Clang)
    };

    struct Settings {
        // CXX_Standard cxx_standard;
        // C_Standard c_standard;
        OptimizationLevel optimization_level;
        bool lto = false;
        bool debug = false;
        bool rpath = false;
        bool debug_assertions = false; // -DNDEBUG
#if defined(_WIN32)
        bool crt_static = true; // Windows: static CRT by default (/MT)
#else
        bool crt_static = false; // Linux/macOS: dynamic libc by default
#endif

        ExceptionHandling exception_handling = ExceptionHandling::Default;

        WarningLevel warning_level = WarningLevel::Off;
    };

    void load(Settings& settings, const toml::value& v);

    void serialize(const Settings& settings, toml::value& out);

    void to_flags(
        const Settings& settings,
        const Compiler& compiler,
        std::vector<std::string>& cflags,
        std::vector<std::string>& lflags);

} // namespace muuk