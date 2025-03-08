#include "../../include/buildparser.hpp"
#include "../../include/buildmanager.h"
#include "../../include/makebackend.hpp"

#include <fstream>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

MakeBackend::MakeBackend(
    muuk::Compiler compiler,
    const std::string& archiver,
    const std::string& linker,
    const std::string& lockfile_path
) : BuildBackend(compiler, archiver, linker, lockfile_path),
build_manager(std::make_shared<BuildManager>()) {
}

void MakeBackend::generate_build_file(
    const std::string& target_build,
    const std::string& profile
) {
    muuk::logger::info("");
    muuk::logger::info("  Generating Makefile for '{}'", profile);
    muuk::logger::info("----------------------------------------");

    build_dir_ = fs::path("build") / profile;
    if (!fs::exists(build_dir_)) {
        fs::create_directories(build_dir_);
        muuk::logger::info("Created build directory: {}", build_dir_.string());
    }

    spdlog::default_logger()->flush();

    build_parser = std::make_shared<BuildParser>(build_manager, muuk_filer, compiler_, build_dir_);

    const std::string makefile = (build_dir_ / "Makefile").string();

    std::ostringstream output_stream;

    write_header(output_stream, profile);
    build_parser->parse();
    generate_build_rules(output_stream, target_build);

    std::ofstream out(makefile, std::ios::out);
    if (!out) {
        throw std::runtime_error("Failed to create Makefile: " + makefile);
    }

    out << output_stream.str();
    out.close();

    muuk::logger::info("Makefile '{}' generated successfully!", makefile);
}

std::string MakeBackend::generate_rule(const CompilationTarget& target) {
    std::ostringstream rule;
    rule << target.output << ": " << target.inputs[0] << "\n";
    rule << "\t$(CXX) $(CXXFLAGS) -c " << target.inputs[0] << " -o " << target.output << "\n";
    return rule.str();
}

std::string MakeBackend::generate_rule(const ArchiveTarget& target) {
    std::ostringstream rule;
    rule << target.output << ":";
    for (const auto& obj : target.inputs) {
        rule << " " << obj;
    }
    rule << "\n";
    rule << "\t$(AR) rcs " << target.output << " " << target.inputs[0] << "\n";
    return rule.str();
}

std::string MakeBackend::generate_rule(const LinkTarget& target) {
    std::ostringstream rule;
    rule << target.output << ":";
    for (const auto& obj : target.inputs) {
        rule << " " << obj;
    }
    rule << "\n";
    rule << "\t$(LINKER) $(LDFLAGS) -o " << target.output << " " << target.inputs[0] << "\n";
    return rule.str();
}

void MakeBackend::write_header(std::ostringstream& out, std::string profile) {
    muuk::logger::info("Writing Makefile header...");

    out << "# ------------------------------------------------------------\n"
        << "# Auto-generated Makefile\n"
        << "# Generated by Muuk\n"
        << "# Profile: " << build_dir_.filename().string() << "\n"
        << "# ------------------------------------------------------------\n\n";

    out << "CXX = " << muuk::compiler::to_string(compiler_) << "\n"
        << "AR = " << archiver_ << "\n"
        << "LINKER = " << linker_ << "\n\n";

    auto [profile_cflags, profile_lflags] = extract_profile_flags(profile);

    out << "CXXFLAGS = " << profile_cflags << "\n"
        << "LDFLAGS = " << profile_lflags << "\n\n";
}

void MakeBackend::generate_build_rules(std::ostringstream& out, const std::string& target_build) {
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
