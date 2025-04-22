#include <string>
#include <vector>

#include "buildtargets.h"

BuildTarget::BuildTarget(std::string target_name, std::string target_output) :
    name(std::move(target_name)), output(std::move(target_output)) {
}

CompilationTarget::CompilationTarget(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags, CompilationUnitType compilation_unit_type_) :
    BuildTarget(obj, obj) {
    input = src;
    inputs = { src };
    flags.insert(flags.end(), cflags.begin(), cflags.end());
    flags.insert(flags.end(), iflags.begin(), iflags.end());
    compilation_unit_type = compilation_unit_type_;
}

ArchiveTarget::ArchiveTarget(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags) :
    BuildTarget(lib, lib) {
    inputs = objs;
    flags = aflags;
}

ExternalTarget::ExternalTarget(
    const std::string& name_,
    const std::string& version_,
    const std::string& type_,
    const std::string& path_,
    const std::vector<std::string>& args_,
    std::vector<std::string>& outputs_) :
    BuildTarget("", "") {
    name = name_;
    version = version_;
    type = type_;
    path = path_;
    args = args_;
    outputs = outputs_;

    output = "";
    flags = args_;
}

LinkTarget::LinkTarget(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags) :
    BuildTarget(exe, exe) {
    inputs = objs;
    inputs.insert(inputs.end(), libs.begin(), libs.end());
    flags = lflags;
}
