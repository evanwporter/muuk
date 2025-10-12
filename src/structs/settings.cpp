#include <string>
#include <toml.hpp>

#include "structs/opt_level.hpp"
#include "structs/settings.hpp"

namespace muuk {
    void load(Settings& settings, const toml::value& v) {
        if (v.contains("opt-level")) {
            // Can be a string
            if (v.at("opt-level").is_string()) {
                settings.optimization_level = opt_lvl_from_string(
                    v.at("opt-level").as_string());
            }

            // Or an integer
            else if (v.at("opt-level").is_integer()) {
                settings.optimization_level = opt_lvl_from_string(
                    std::to_string(v.at("opt-level").as_integer()));
            }
        }

        if (v.contains("lto"))
            settings.lto = v.at("lto").as_boolean();
        if (v.contains("debug"))
            settings.debug = v.at("debug").as_boolean();
        if (v.contains("rpath"))
            settings.rpath = v.at("rpath").as_boolean();
        if (v.contains("debug-assertions"))
            settings.debug_assertions = v.at("debug-assertions").as_boolean();
        if (v.contains("crt-static"))
            settings.crt_static = v.at("crt-static").as_boolean();

        if (v.contains("exceptions")) {
            const auto& ex_val = v.at("exceptions");

            if (ex_val.is_integer()) {
                int val = static_cast<int>(ex_val.as_integer());
                if (val < 0)
                    val = 0;
                if (val > 2)
                    val = 2;
                settings.exception_handling = static_cast<ExceptionHandling>(val);
            } else if (ex_val.is_string()) {
                const std::string ex = ex_val.as_string();
                if (ex == "none")
                    settings.exception_handling = ExceptionHandling::None;
                else if (ex == "async")
                    settings.exception_handling = ExceptionHandling::Asynchronous;
                else
                    settings.exception_handling = ExceptionHandling::Default;
            }
        }

        if (v.contains("warning-level")) {
            int level = 0;
            if (v.at("warning-level").is_integer())
                level = static_cast<int>(v.at("warning-level").as_integer());
            else if (v.at("warning-level").is_string()) {
                const std::string str = v.at("warning-level").as_string();
                try {
                    level = std::stoi(str);
                } catch (...) {
                    if (str == "off")
                        level = 0;
                }
            }

            // Clamp or validate
            if (level < 0)
                level = 0;
            if (level > 4)
                level = 4;

            settings.warning_level = static_cast<WarningLevel>(level);
        }
    }

    void serialize(const Settings& settings, toml::value& out) {
        out["opt-level"] = to_string(settings.optimization_level);
        out["lto"] = settings.lto;
        out["debug"] = settings.debug;
        out["rpath"] = settings.rpath;
        out["debug-assertions"] = settings.debug_assertions;
        out["crt-static"] = settings.crt_static;
        out["exceptions"] = static_cast<int>(settings.exception_handling);
        out["warning-level"] = static_cast<int>(settings.warning_level);
    }

    void to_flags(const Settings& settings, const Compiler& compiler, std::vector<std::string>& cflags, std::vector<std::string>& lflags) {

        // --- Link Time Optimization ---
        if (settings.lto) {
            switch (compiler.getType()) {
            case Compiler::Type::GCC:
            case Compiler::Type::Clang:
                cflags.push_back("-flto");
                lflags.push_back("-flto");
                break;
            case Compiler::Type::MSVC:
                cflags.push_back("/GL");
                lflags.push_back("/LTCG");
                break;
            }
        }

        // --- Debug ---
        if (settings.debug) {
            switch (compiler.getType()) {
            case Compiler::Type::GCC:
            case Compiler::Type::Clang:
                cflags.push_back("-g");
                break;
            case Compiler::Type::MSVC:
                cflags.push_back("/Zi");
                lflags.push_back("/DEBUG");
                break;
            }
        }

        // --- Debug Assertions ---
        if (!settings.debug_assertions) {
            switch (compiler.getType()) {
            case Compiler::Type::GCC:
            case Compiler::Type::Clang:
                cflags.push_back("-DNDEBUG");
                break;
            case Compiler::Type::MSVC:
                cflags.push_back("/DNDEBUG");
                break;
            }
        }

        // --- CRT Static ---
        switch (compiler.getType()) {
        case Compiler::Type::MSVC:
            if (settings.crt_static)
                cflags.push_back(settings.debug ? "/MTd" : "/MT");
            else
                cflags.push_back(settings.debug ? "/MDd" : "/MD");
            break;
        case Compiler::Type::GCC:
        case Compiler::Type::Clang:
            if (settings.crt_static) {
                lflags.push_back("-static-libstdc++");
                lflags.push_back("-static-libgcc");
            }
            break;
        }

        // --- Exception Handling ---
        switch (settings.exception_handling) {
        case muuk::ExceptionHandling::None:
            switch (compiler.getType()) {
            case Compiler::Type::MSVC:
                cflags.push_back("/EHs-");
                cflags.push_back("/D_HAS_EXCEPTIONS=0");
                break;
            case Compiler::Type::GCC:
            case Compiler::Type::Clang:
                cflags.push_back("-fno-exceptions");
                break;
            }
            break;

        case muuk::ExceptionHandling::Asynchronous:
            if (compiler.getType() == Compiler::Type::MSVC)
                cflags.push_back("/EHa");
            else
                cflags.push_back("-fnon-call-exceptions");
            break;

        case muuk::ExceptionHandling::Default:
        default:
            if (compiler.getType() == Compiler::Type::MSVC)
                cflags.push_back("/EHsc");
            else
                cflags.push_back("-fexceptions");
            break;
        }

        // --- Optimization Level ---
        cflags.push_back(
            to_flag(settings.optimization_level, compiler.getType()));
    }

};