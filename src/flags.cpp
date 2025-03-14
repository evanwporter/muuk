#include "muuk.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <ctre.hpp>
#include <unordered_map>

namespace muuk {
    std::string normalize_flag(const std::string& flag, const Compiler compiler) {
        std::string normalized_flag = flag;

        if (flag[0] != '/' && flag[0] != '-') {
            if (compiler == Compiler::MSVC) {
                normalized_flag = "/" + flag;
            }
            else {
                normalized_flag = "-" + flag;
            }
        }

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
            {"/sdl", "-D_FORTIFY_SOURCE=2"},
            {"/DEBUG", "-g" },
            {"/MACHINE:X86", "-m64"},
            {"/std:20", "-std=20"}
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

        if (flag.starts_with("/D") || flag.starts_with("-D")) {
            if (compiler == Compiler::MSVC) {
                return "/D" + flag.substr(2);  // MSVC uses /D
            }
            else {
                return "-D" + flag.substr(2);  // GCC uses -D
            }
        }

        if (compiler == Compiler::MSVC) {
            auto lookup = gcc_to_msvc.find(normalized_flag);
            if (lookup != gcc_to_msvc.end()) {
                return lookup->second;
            }
        }
        else {
            auto lookup = msvc_to_gcc.find(normalized_flag);
            if (lookup != msvc_to_gcc.end()) {
                return lookup->second;
            }
        }

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
    std::string normalize_flags(const std::vector<std::string>& flags, const Compiler compiler) {
        std::string normalized;
        for (const auto& flag : flags) {
            normalized += " " + normalize_flag(flag, compiler);
        }
        return normalized;
    }

    // Normalize a vector of flags in-place
    void normalize_flags_inplace(std::vector<std::string>& flags, const Compiler compiler) {
        for (auto& flag : flags) {
            flag = normalize_flag(flag, compiler);
        }
    }
}
