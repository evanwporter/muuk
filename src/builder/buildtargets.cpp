#include <sstream>
#include <string>

#include "buildtargets.h"

BuildTarget::BuildTarget(std::string target_name, std::string target_output) :
    name(std::move(target_name)), output(std::move(target_output)) {
}

CompilationTarget::CompilationTarget(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags) :
    BuildTarget(obj, obj) {
    inputs = { src };
    flags.insert(flags.end(), cflags.begin(), cflags.end());
    flags.insert(flags.end(), iflags.begin(), iflags.end());
}

std::string CompilationTarget::generate_ninja_rule() const {
    std::ostringstream rule;
    rule << "build " << output << ": compile " << inputs[0];

    if (!dependencies.empty()) {
        rule << " |";
        for (const auto& dep : dependencies) {
            rule << " " << dep;
        }
    }

    rule << "\n";
    if (!flags.empty()) {
        rule << "  cflags =";
        for (const auto& flag : flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}

ArchiveTarget::ArchiveTarget(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags) :
    BuildTarget(lib, lib) {
    inputs = objs;
    flags = aflags;
}

std::string ArchiveTarget::generate_ninja_rule() const {
    std::ostringstream rule;
    rule << "build " << output << ": archive";
    for (const auto& obj : inputs) {
        rule << " " << obj;
    }
    rule << "\n";

    if (!flags.empty()) {
        rule << "  aflags =";
        for (const auto& flag : flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}

LinkTarget::LinkTarget(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags) :
    BuildTarget(exe, exe) {
    inputs = objs;
    inputs.insert(inputs.end(), libs.begin(), libs.end());
    flags = lflags;
}

std::string LinkTarget::generate_ninja_rule() const {
    std::ostringstream rule;
    rule << "build " << output << ": link";
    for (const auto& obj : inputs) {
        rule << " " << obj;
    }
    rule << "\n";

    if (!flags.empty()) {
        rule << "  lflags =";
        for (const auto& flag : flags) {
            rule << " " << flag;
        }
        rule << "\n";
    }
    return rule.str();
}
