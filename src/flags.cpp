#include <vector>
#include <string>
#include <unordered_map>
#include <ctre.hpp>
#include <iostream>
#include <unordered_map>

#include "muuk.h"

namespace muuk {
    std::string normalize_flag(const std::string& flag, const compiler::Compiler compiler) {
        static const std::unordered_map<std::string, std::string> msvc_to_gcc = {
            {"/I", "-I"},
            {"/Fe", "-o"},
            {"/Fo", "-o"},
            {"/c", "-c"},
            {"/W0", "-w"},
            {"/W1", "-Wall"},
            {"/W2", "-Wall -Wextra"},
            {"/W3", "-Wall -Wextra -Wpedantic"},
            {"/W4", "-Wall -Wextra -Wpedantic -Wconversion"},
            {"/EHsc", "-fexceptions"},
            {"/Zi", "-g"},
            {"/O2", "-O2"},
            {"/O3", "-O3"},
            {"/GL", "-flto"},
            {"/link", "-Wl,"},
            {"/utf-8", "-finput-charset=UTF-8"},
            {"/D", "-D"},
            {"/FS", ""},
            {"/Od", "-O0"},
            {"/RTC1", "-fstack-protector"},
            {"/RTCc", "-ftrapv"},
            {"/Ob0", "-fno-inline"},
            {"/Ob1", "-finline-functions"},
            {"/Ob2", "-finline-functions -finline-small-functions"},
            {"/LTCG", "-flto"},
            {"/MT", "-static-libgcc -static-libstdc++"},
            {"/MP", "-pipe"},
            {"/GR", "-frtti"},
            {"/GR-", "-fno-rtti"},
            {"/fp:fast", "-ffast-math"},
            {"/fp:precise", "-fexcess-precision=standard"},
            {"/arch:AVX", "-mavx"},
            {"/arch:AVX2", "-mavx2"},
            {"/arch:SSE2", "-msse2"},
            {"/arch:SSE3", "-msse3"},
            {"/LD", "-shared"},
            {"/INCREMENTAL:NO", "-Wl,--no-incremental"},
            {"/OPT:REF", "-Wl,--gc-sections"},
            {"/OPT:ICF", "-Wl,--icf=safe"},
            {"/SUBSYSTEM:CONSOLE", "-Wl,-subsystem,console"},
            {"/SUBSYSTEM:WINDOWS", "-Wl,-subsystem,windows"},
            {"/GS", "-fstack-protector-strong"},
            {"/sdl", "-D_FORTIFY_SOURCE=2"}
        };

        static const std::unordered_map<std::string, std::string> gcc_to_msvc = {
            {"-I", "/I"},
            {"-o", "/Fe"},
            {"-c", "/c"},
            {"-w", "/W0"},
            {"-Wall", "/W3"},
            {"-Wextra", "/W4"},
            {"-Wpedantic", "/W4"},
            {"-Wconversion", "/W4"},
            {"-fexceptions", "/EHsc"},
            {"-g", "/Zi"},
            {"-O2", "/O2"},
            {"-O3", "/O3"},
            {"-flto", "/GL"},
            {"-Wl,", "/link"},
            {"-finput-charset=UTF-8", "/utf-8"},
            {"-D", "/D"},
            {"-O0", "/Od"}
        };

        constexpr auto std_pattern = ctll::fixed_string{ R"((?:/std:c\+\+|-std=c\+\+)(\d+))" };
        std::string normalized_flag = flag;

        if (flag.starts_with("/D") || flag.starts_with("-D")) {
#ifdef _WIN32
            return "/D" + flag.substr(2);  // MSVC uses /D
#else
            return "-D" + flag.substr(2);  // GCC uses -D
#endif
        }

        if (flag[0] != '/' && flag[0] != '-') {
#ifdef _WIN32
            normalized_flag = "/" + flag;
#else
            normalized_flag = "-" + flag;
#endif
        }

#ifdef _WIN32
        auto lookup = gcc_to_msvc.find(normalized_flag);
        if (lookup != gcc_to_msvc.end()) {
            return lookup->second;
        }
#else
        auto lookup = msvc_to_gcc.find(normalized_flag);
        if (lookup != msvc_to_gcc.end()) {
            return lookup->second;
        }
#endif

        // Handle C++ standard flag conversion (-std=c++20 <-> /std:c++20)
        if (auto match = ctre::match<std_pattern>(flag)) {
#ifdef _WIN32
            return "/std:c++" + std::string(match.get<1>());
#else
            return "-std=c++" + std::string(match.get<1>());
#endif
        }
        return normalized_flag;
    }

    // Normalize a vector of flags
    std::string normalize_flags(const std::vector<std::string>& flags, const compiler::Compiler compiler) {
        std::string normalized;
        for (const auto& flag : flags) {
            normalized += " " + normalize_flag(flag, compiler);
        }
        return normalized;
    }
}
