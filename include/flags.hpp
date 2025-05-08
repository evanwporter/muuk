#include <string>
#include <vector>

#include <compiler.hpp>

namespace muuk {
    enum class FlagCategory {
        Include,
        Defines,
        Optimization,
        Debugging,
        Output,
        Warnings,
        Advanced,
        Modules,
        Version,
        Logo
    };

    struct FlagInfo {
        std::string canonical;
        FlagCategory category;
        std::tuple<std::string, std::string, std::string> equivalents; // GCC, Clang, MSVC
    };

    const std::vector<FlagInfo> flag_table = {
        { "include_path", FlagCategory::Include, { "-I", "-I", "/I" } },
        { "include_system", FlagCategory::Include, { "-isystem", "-isystem", "" } },

        { "define_macro", FlagCategory::Defines, { "-D", "-D", "/D" } },
        { "undefine_macro", FlagCategory::Defines, { "-U", "-U", "/U" } },

        { "opt_O0", FlagCategory::Optimization, { "-O0", "-O0", "/Od" } },
        { "opt_O1", FlagCategory::Optimization, { "-O1", "-O1", "" } },
        { "opt_O2", FlagCategory::Optimization, { "-O2", "-O2", "/O2" } },
        { "opt_O3", FlagCategory::Optimization, { "-O3", "-O3", "/Ox" } },
        { "opt_Og", FlagCategory::Optimization, { "-Og", "-Og", "/Od" } },
        { "opt_Os", FlagCategory::Optimization, { "-Os", "-Os", "/O1" } },
        { "opt_Ofast", FlagCategory::Optimization, { "-Ofast", "-Ofast", "/fp:fast /Ox" } },

        { "debug_symbols", FlagCategory::Debugging, { "-g", "-g", "/Zi" } },
        { "debug_macro_info", FlagCategory::Debugging, { "-g3", "-g3", "" } },
        { "no_debug_info", FlagCategory::Debugging, { "-g0", "-g0", "/DEBUG:NONE" } },
        { "pdb_output", FlagCategory::Debugging, { "", "", "/Fdfile.pdb" } },

        { "compile_only", FlagCategory::Output, { "-c", "-c", "/c" } },
        { "specify_output", FlagCategory::Output, { "-o", "-o", "/Fo" } },
        { "assembly_output", FlagCategory::Output, { "-S", "-S", "/FA" } },
        { "preprocess_only", FlagCategory::Output, { "-E", "-E", "/EP" } },

        { "warn_all", FlagCategory::Warnings, { "-Wall", "-Wall", "/W3" } },
        { "warn_extra", FlagCategory::Warnings, { "-Wextra", "-Wextra", "/W4" } },
        { "warn_error", FlagCategory::Warnings, { "-Werror", "-Werror", "/WX" } },

        { "lto", FlagCategory::Advanced, { "-flto", "-flto", "/GL" } },
        { "march_native", FlagCategory::Advanced, { "-march=native", "-march=native", "/arch:AVX2" } },
        { "cpp_std_17", FlagCategory::Advanced, { "-std=c++17", "-std=c++17", "/std:c++17" } },
        { "no_exceptions", FlagCategory::Advanced, { "-fno-exceptions", "-fno-exceptions", "/EHs-c-" } },
        { "no_rtti", FlagCategory::Advanced, { "-fno-rtti", "-fno-rtti", "/GR-" } },

        { "modules_enable", FlagCategory::Modules, { "-fmodules-ts", "-fmodules", "/experimental:module" } },
        { "module_interface_compile", FlagCategory::Modules, { "-x c++-module", "-fmodules", "/interface" } },
        { "module_output", FlagCategory::Modules, { "-o mymod.gcm", "-o mymod.pcm", "mymod.ifc" } },
        { "module_cache_path", FlagCategory::Modules, { "-fmodules-cache-path=", "-fmodules-cache-path=", "/ifcOutput" } },

        { "standard_version", FlagCategory::Version, { "-std=c++", "-std=c++", "/std:c++" } },

        { "no_logo", FlagCategory::Logo, { "", "", "/nologo" } },

    };
};