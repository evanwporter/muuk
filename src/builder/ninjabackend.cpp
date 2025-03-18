#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "buildbackend.hpp"
#include "buildmanager.h"
#include "buildparser.hpp"
#include "buildtargets.h"
#include "logger.h"
#include "util.h"

namespace fs = std::filesystem;

NinjaBackend::NinjaBackend(
    muuk::Compiler compiler,
    const std::string& archiver,
    const std::string& linker,
    const std::string& lockfile_path) :
    BuildBackend(compiler, archiver, linker, lockfile_path),
    build_manager(std::make_shared<BuildManager>()) {
}

void NinjaBackend::generate_build_file(
    const std::string& target_build,
    const std::string& profile) {
    muuk::logger::info("");
    muuk::logger::info("  Generating Ninja file for '{}'", profile);
    muuk::logger::info("----------------------------------------");

    build_dir_ = fs::path("build") / profile;
    if (!fs::exists(build_dir_)) {
        fs::create_directories(build_dir_);
        muuk::logger::info("Created build directory: {}", build_dir_.string());
    }

    spdlog::default_logger()->flush();

    build_parser = std::make_shared<BuildParser>(build_manager, muuk_filer, compiler_, build_dir_, profile);

    const std::string ninja_file_ = (build_dir_ / "build.ninja").string();

    std::ostringstream output_stream;

    write_header(output_stream, profile);
    build_parser->parse();
    generate_build_rules(output_stream, target_build);

    std::ofstream out(ninja_file_, std::ios::out);
    if (!out) {
        throw std::runtime_error("Failed to create Ninja build file: " + ninja_file_);
    }

    out << output_stream.str();
    out.close();

    muuk::logger::info("Ninja build file '{}' generated successfully!", ninja_file_);
}

std::string NinjaBackend::generate_rule(const CompilationTarget& target) {
    std::ostringstream rule;
    rule << "build " << target.output << ": compile " << target.inputs[0];

    if (!target.dependencies.empty()) {
        rule << " |";
        for (const auto& dep : target.dependencies) {
            rule << " " << dep;
        }
    }

    rule << "\n";
    if (!target.flags.empty()) {
        rule << "  cflags =";
        for (const auto& flag : target.flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}

std::string NinjaBackend::generate_rule(const ArchiveTarget& target) {
    std::ostringstream rule;
    rule << "build " << target.output << ": archive";
    for (const auto& obj : target.inputs) {
        rule << " " << obj;
    }
    rule << "\n";

    if (!target.flags.empty()) {
        rule << "  aflags =";
        for (const auto& flag : target.flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}

std::string NinjaBackend::generate_rule(const LinkTarget& target) {
    std::ostringstream rule;
    rule << "build " << target.output << ": link";
    for (const auto& obj : target.inputs) {
        rule << " " << obj;
    }
    rule << "\n";

    if (!target.flags.empty()) {
        rule << "  lflags =";
        for (const auto& flag : target.flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}

void NinjaBackend::write_header(std::ostringstream& out, std::string profile) {
    muuk::logger::info("Writing Ninja header...");

    out << "# ------------------------------------------------------------\n"
        << "# Auto-generated Ninja build file\n"
        << "# Generated by Muuk\n"
        << "# Profile: " << build_dir_.filename().string() << "\n"
        << "# ------------------------------------------------------------\n\n";

    out << "# Toolchain Configuration\n"
        << "cxx = " << compiler_.to_string() << "\n"
        << "ar = " << archiver_ << "\n"
        << "linker = " << linker_ << "\n\n";

    std::string module_dir = util::to_linux_path((build_dir_ / "modules/").string());

    util::ensure_directory_exists(module_dir);
    muuk::logger::info("Created module build directory: {}", module_dir);

    module_dir = "../../" + module_dir;

    auto [profile_cflags, profile_lflags] = build_parser->extract_profile_flags(profile);

    out << "# Profile-Specific Flags\n"
        << "profile_cflags = " << profile_cflags << "\n"
        << "profile_lflags = " << profile_lflags << "\n\n";

    out << "# ------------------------------------------------------------\n"
        << "# Rules for Compiling C++ Modules\n"
        << "# ------------------------------------------------------------\n";

    if (compiler_ == muuk::Compiler::MSVC) {
        // MSVC Compiler
        out << "rule module_compile\n"
            << "  command = $cxx /std:c++20 /utf-8 /c /interface $in /Fo$out /IFC:"
            << module_dir << " /ifcOutput "
            << module_dir << " /ifcSearchDir "
            << module_dir << " $cflags $profile_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    } else if (compiler_ == muuk::Compiler::Clang) {
        // Clang Compiler
        out << "rule module_compile\n"
            << "  command = $cxx -std=c++20 -fmodules-ts -c $in -o $out -fmodule-output="
            << module_dir << " $cflags $profile_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    } else if (compiler_ == muuk::Compiler::GCC) {
        // GCC Compiler
        out << "rule module_compile\n"
            << "  command = $cxx -std=c++20 -fmodules-ts -c $in -o $out -fmodule-output="
            << module_dir << " $cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    } else {
        muuk::logger::error("Unsupported compiler: {}", compiler_.to_string());
        throw std::invalid_argument("Unsupported compiler: " + compiler_.to_string());
    }

    out << "# ------------------------------------------------------------\n"
        << "# Compilation, Archiving, and Linking Rules\n"
        << "# ------------------------------------------------------------\n";

    if (compiler_ == muuk::Compiler::MSVC) {
        // MSVC (cl)
        out << "rule compile\n"
            << "  command = $cxx /c $in"
            << " /Fo$out $profile_cflags $platform_cflags $cflags /showIncludes\n"
            << "  deps = msvc\n"
            << "  description = Compiling $in\n\n"

            << "rule archive\n"
            << "  command = $ar /OUT:$out $in\n"
            << "  description = Archiving $out\n\n"

            << "rule link\n"
            << "  command = $linker $in /OUT:$out $profile_lflags $lflags $libraries\n"
            << "  description = Linking $out\n\n";
    } else {
        // MinGW or Clang on Windows / Unix
        out << "rule compile\n"
            << "  command = $cxx -c $in -o $out $profile_cflags $platform_cflags $cflags\n"
            << "  description = Compiling $in\n\n"

            << "rule archive\n"
            << "  command = $ar rcs $out $in\n"
            << "  description = Archiving $out\n\n"

            << "rule link\n"
            << "  command = $linker $in -o $out $profile_lflags $lflags $libraries\n"
            << "  description = Linking $out\n\n";
    }
}

void NinjaBackend::generate_build_rules(std::ostringstream& out, const std::string& target_build) {
    (void)target_build;
    std::ostringstream build_rules;

    // Generate compilation rules
    for (const auto& target : build_manager->get_compilation_targets()) {
        build_rules << generate_rule(target);
    }

    build_rules << "\n";

    // Generate archive rules
    for (const auto& target : build_manager->get_archive_targets()) {
        build_rules << generate_rule(target);
    }

    build_rules << "\n";

    // Generate link rules
    for (const auto& target : build_manager->get_link_targets()) {
        build_rules << generate_rule(target);
    }

    // Write build rules to the file
    out << build_rules.str();
}