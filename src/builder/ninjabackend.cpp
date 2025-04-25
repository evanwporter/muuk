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
    BuildManager& build_manager,
    muuk::Compiler compiler,
    const std::string& archiver,
    const std::string& linker) :
    BuildBackend(build_manager, compiler, archiver, linker) { }

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

    const std::string ninja_file_ = (build_dir_ / "build.ninja").string();

    std::ostringstream output_stream;

    write_header(output_stream, profile);
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

    const bool is_module = target.compilation_unit_type == CompilationUnitType::Module;
    std::string module_output;
    if (is_module) {
        // Use logical_name or derive .ifc file name
        module_output = "../../" + (build_dir_ / "modules" / (target.logical_name + ".ifc")).string();
        module_output = util::file_system::to_linux_path(module_output);
    }

    if (is_module) {
        rule << "build " << module_output << ": compile_module " << target.inputs[0] << "\n";

        if (!target.flags.empty()) {
            rule << "  cflags =";
            for (const auto& flag : target.flags)
                rule << " " << flag;
            rule << "\n";
        }

        rule << "\n";
    }

    rule << "build " << target.output << ": compile " << target.inputs[0];

    if (is_module)
        rule << " | " << module_output; // Ensure dependency on module rule

    if (!target.dependencies.empty()) {
        rule << " |";
        for (const auto& dep : target.dependencies)
            rule << " ../../" << dep;
    }

    rule << "\n";
    if (!target.flags.empty()) {
        rule << "  cflags =";
        for (const auto& flag : target.flags)
            rule << " " << flag;

        rule << "\n";
    }
    return rule.str();
}

std::string NinjaBackend::generate_rule(const ArchiveTarget& target) {
    std::ostringstream rule;
    rule << "build " << target.output << ": archive";
    for (const auto& obj : target.inputs)
        rule << " " << obj;
    rule << "\n";

    if (!target.flags.empty()) {
        rule << "  aflags =";
        for (const auto& flag : target.flags)
            rule << " " << flag;
        rule << "\n";
    }
    return rule.str();
}

void NinjaBackend::generate_rule(const ExternalTarget& target) {
    fs::path folder_path = fs::absolute(target.path);
    std::string safe_id = "ext_" + target.name;
    fs::path output_path = build_dir_ / (safe_id + ".ninja");

    std::ostringstream out;

    out << "# ----------------------------------------\n"
        << "# External Project: " << target.name << "\n"
        << "# ----------------------------------------\n\n";

    // Configure args
    std::string configure_args;
    for (const auto& arg : target.args)
        configure_args += " " + arg;

    std::string configure_stamp = (folder_path / "build" / "build.ninja").string();

    out << "build " << configure_stamp << ": configure_external " << folder_path / "CMakeLists.txt"
        << "\n";
    out << "  configure_args =" << configure_args << "\n\n";

    out << "build build_" << safe_id << ": build_external || " << configure_stamp << "\n";
    out << "  description = Building " << target.name << "\n\n";

    // Final alias
    out << "build " << safe_id << ": phony build_" << safe_id << "\n";

    std::ofstream fout(output_path);
    if (!fout.is_open())
        throw std::runtime_error("Failed to write external build file: " + output_path.string());

    fout << out.str();
    fout.close();

    muuk::logger::info("Generated external Ninja file: {}", output_path.string());
}

std::string NinjaBackend::generate_rule(const LinkTarget& target) {
    std::ostringstream rule;
    rule << "build " << target.output << ": link";
    for (const auto& obj : target.inputs)
        rule << " " << obj;

    rule << "\n";

    if (!target.flags.empty()) {
        rule << "  lflags =";
        for (const auto& flag : target.flags)
            rule << " " << flag;

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

    std::string module_dir = util::file_system::to_linux_path((build_dir_ / "modules/").string());

    util::file_system::ensure_directory_exists(module_dir);
    muuk::logger::info("Created module build directory: {}", module_dir);

    module_dir = "../../" + module_dir;

    auto [profile_cflags, profile_lflags] = get_profile_flag_strings(build_manager, profile);

    out << "# Profile-Specific Flags\n"
        << "profile_cflags = " << profile_cflags << "\n"
        << "profile_lflags = " << profile_lflags << "\n\n";

    out << "# ------------------------------------------------------------\n"
        << "# Rules for Compiling C++ Modules\n"
        << "# ------------------------------------------------------------\n";

    if (compiler_ == muuk::Compiler::MSVC) {
        // MSVC Compiler
        out << "rule compile_module\n"
            << "  command = $cxx /std:c++20 /utf-8 /c $in /ifcOnly"
            << " /ifcOutput "
            << module_dir << " /ifcSearchDir "
            << module_dir << " $cflags $profile_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    } else if (compiler_ == muuk::Compiler::Clang) {
        // Clang Compiler
        out << "rule compile_module\n"
            << "  command = $cxx -x c++-module -std=c++20 -fmodules-ts -c $in -o $out -fmodule-output="
            << module_dir << " $cflags $profile_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    } else if (compiler_ == muuk::Compiler::GCC) {
        // GCC Compiler
        out << "rule compile_module\n"
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
            << " /Fo$out $profile_cflags $platform_cflags $cflags /showIncludes "
            << "/ifcSearchDir " << module_dir << "\n"
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

    out << "# ------------------------------------------------------------\n"
        << "# Rules for External Targets\n"
        << "# ------------------------------------------------------------\n";

    out << "rule configure_external\n"
        << "  command = cmake -B build -S . -G Ninja $configure_args\n"
        << "  description = Configuring external project\n\n";

    out << "rule build_external\n"
        << "  command = ninja -C build\n"
        << "  description = Building external project\n\n";
}

void NinjaBackend::generate_build_rules(std::ostringstream& out, const std::string& target_build) {
    (void)target_build;
    std::ostringstream build_rules;

    // Generate compilation rules
    for (const auto& target : build_manager.get_compilation_targets())
        build_rules << generate_rule(target);

    build_rules << "\n";

    // Generate archive rules
    for (const auto& target : build_manager.get_archive_targets())
        build_rules << generate_rule(target);

    build_rules << "\n";

    // Generate link rules
    for (const auto& target : build_manager.get_link_targets())
        build_rules << generate_rule(target);

    // Write build rules to the file
    out << build_rules.str();
}
