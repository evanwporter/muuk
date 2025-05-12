#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "build/backend.hpp"
#include "build/manager.hpp"
#include "build/parser.hpp"
#include "build/targets.hpp"
#include "logger.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {
    namespace build {
        NinjaBackend::NinjaBackend(
            const BuildManager& build_manager,
            const muuk::Compiler compiler,
            const std::string& archiver,
            const std::string& linker) :
            BuildBackend(build_manager, compiler, archiver, linker) { }

        void NinjaBackend::generate_build_file(
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
            generate_build_rules(output_stream);

            std::ofstream out(ninja_file_, std::ios::out);
            if (!out) {
                throw std::runtime_error("Failed to create Ninja build file: " + ninja_file_);
            }

            out << output_stream.str();
            out.close();

            muuk::logger::info("Ninja build file '{}' generated successfully!", ninja_file_);
        }

        std::string NinjaBackend::generate_rule(const CompilationTarget& target) const {
            std::ostringstream rule;

            const bool is_module = target.compilation_unit_type == CompilationUnitType::Module;
            std::string module_output;
            if (is_module) {
                // Use logical_name or derive .ifc file name
                module_output = "../../" + (build_dir_ / "modules" / (target.logical_name + ".ifc")).string();
                module_output = util::file_system::to_linux_path(module_output);
            }

            if (is_module) {
                rule << "build " << module_output << ": compile_module "
                     << util::file_system::escape_drive_letter(target.inputs[0])
                     << "\n";

                if (!target.flags.empty()) {
                    rule << "  cflags =";
                    for (const auto& flag : target.flags)
                        rule << " " << flag;
                    rule << "\n";
                }

                rule << "\n";
            }

            if (compiler_ == muuk::Compiler::Clang && is_module) {
                rule << "build " << target.output << ": compile " << util::file_system::escape_drive_letter(module_output);
            } else {
                rule << "build " << target.output << ": compile " << util::file_system::escape_drive_letter(target.inputs[0]);
            }

            if (is_module)
                rule << " | " << module_output; // Ensure dependency on module rule

            if (!target.dependencies.empty()) {
                rule << " |";
                for (const auto& dep : target.dependencies)
                    rule << " ../../" << util::file_system::to_linux_path((build_dir_ / "modules" / (dep->logical_name + ".ifc")).string());
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

        std::string NinjaBackend::generate_rule(const ArchiveTarget& target) const {
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

        std::string NinjaBackend::generate_rule(const ExternalTarget& target) const {
            const std::string safe_id = "ext_" + target.name;
            const fs::path output_path = build_dir_ / (safe_id + ".ninja");

            std::ostringstream out;

            // Configure args
            std::string configure_args;
            for (const auto& arg : target.args)
                configure_args += " " + arg;

            out << "build " << target.cache_file << ": configure_external " << target.source_file << "\n"
                << "  build_dir = " << target.build_path << "\n"
                << "  source_dir = " << target.source_path << "\n"
                << "  configure_args = " << configure_args << "\n";

            out << "build " << target.outputs[0] << " : build_external " << target.cache_file << "\n"
                << "  build_dir = " << target.build_path << "\n\n";

            muuk::logger::info("Generated external Ninja file: {}", output_path.string());

            return out.str();
        }

        std::string NinjaBackend::generate_rule(const LinkTarget& target) const {
            std::ostringstream rule;

            switch (target.link_type) {
            case BuildLinkType::STATIC:
                rule << "build " << target.output << ": archive";
                break;

            case BuildLinkType::SHARED:
                rule << "build " << target.output << ": link_shared";
                break;

            case BuildLinkType::EXECUTABLE:
            default:
                rule << "build " << target.output << ": link";
                break;
            }

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

        void NinjaBackend::write_header(std::ostringstream& out, std::string profile) const {
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

            const auto [profile_cflags, profile_aflags, profile_lflags]
                = get_profile_flag_strings(build_manager, profile);

            out << "# Profile-Specific Flags\n"
                << "profile_cflags = " << profile_cflags << "\n"
                << "profile_aflags = " << profile_aflags << "\n"
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
                // -x c++-module is used to specify that the input file is a module (ie: when it doesn't end with .cppm)
                out << "rule compile_module\n"
                    << "  command = $cxx -x c++-module -std=c++20 --precompile "
                    << "-fprebuilt-module-path=" << module_dir << " "
                    << "$in -o $out $cflags $profile_cflags\n"
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
                << "# Rules\n"
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
                    << "  command = $ar /OUT:$out $in $aflags $profile_aflags\n"
                    << "  description = Archiving $out\n\n"

                    << "rule link\n"
                    << "  command = $linker $in /OUT:$out $lflags $profile_lflags $libraries\n"
                    << "  description = Linking $out\n\n"

                    << "rule link_shared\n"
                    << "  command = $linker $in /DLL /OUT:$out $lflags $profile_lflags $libraries\n"
                    << "  description = Linking shared library $out\n\n";

            } else {
                // MinGW or Clang on Windows / Unix
                out << "rule compile\n"
                    << "  command = $cxx -c $in -o $out $profile_cflags $platform_cflags $cflags\n"
                    << "  description = Compiling $in\n\n"

                    << "rule archive\n"
                    << "  command = $ar rcs $out $in $aflags $profile_aflags\n"
                    << "  description = Archiving $out\n\n"

                    << "rule link\n"
                    << "  command = $linker $in -o $out $lflags $profile_lflags $libraries\n"
                    << "  description = Linking $out\n\n"

                    << "rule link_shared\n"
                    << "  command = $cxx -shared $in -o $out $lflags $profile_lflags $libraries\n"
                    << "  description = Linking shared library $out\n\n";
            }

            std::string cmake_build_type;

            if (profile == "release")
                cmake_build_type = "Release";
            else if (profile == "debug")
                cmake_build_type = "Debug";

            out << "rule configure_external\n"
                << "  command = cmake -B $build_dir -S $source_dir -G Ninja $configure_args -DCMAKE_BUILD_TYPE=" << cmake_build_type << "\n"
                << "  description = Configuring external project\n\n";

            out << "rule build_external\n"
                << "  command = ninja -C $build_dir\n"
                << "  description = Building external project\n\n";
        }

        void NinjaBackend::generate_build_rules(std::ostringstream& out) const {
            std::ostringstream build_rules;
            std::ostringstream phony_rules;

            // Generate compilation rules
            build_rules << "# ----------------------------------\n"
                        << "# Compililed Targets\n"
                        << "# ----------------------------------\n";
            for (const auto& target : build_manager.get_compilation_targets())
                build_rules << generate_rule(target);

            build_rules << "\n";

            // Generate archive rules
            build_rules << "# ----------------------------------\n"
                        << "# Archived Targets\n"
                        << "# ----------------------------------\n";
            for (const auto& target : build_manager.get_archive_targets())
                build_rules << generate_rule(target);

            build_rules << "\n";

            // Generate external_archive rules
            build_rules << "# ----------------------------------\n"
                        << "# External Targets\n"
                        << "# ----------------------------------\n";
            for (const auto& target : build_manager.get_external_targets())
                build_rules << generate_rule(target);

            build_rules << "\n";

            // Generate link rules
            build_rules << "# ----------------------------------\n"
                        << "# Link Targets\n"
                        << "# ----------------------------------\n";
            for (const auto& target : build_manager.get_link_targets()) {
                build_rules << generate_rule(target);

                // Generate phony alias
                // (e.g. `muuk.exe` -> `muuk`)
                std::string short_name = fs::path(target.output).stem().string();
                phony_rules << "build " << short_name << ": phony " << target.output << "\n";
            }

            build_rules << "\n"
                        << phony_rules.str();

            // Write build rules to the file
            out << build_rules.str();
        }
    } // namespace build
} // namespace muuk