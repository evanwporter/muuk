#include <string>
#include <vector>

#include "build/targets.hpp"
#include "util.hpp"

namespace muuk {
    namespace build {
        BuildTarget::BuildTarget(const std::string target_name, const std::string target_output) :
            name(std::move(target_name)), output(std::move(target_output)) {
        }

        CompilationTarget::CompilationTarget(const std::string src, const std::string obj, const CompilationFlags compilation_flags, CompilationUnitType compilation_unit_type_) :
            BuildTarget(obj, obj) {
            input = src;
            inputs = { src };

            using util::array_ops::merge;
            merge(flags, compilation_flags.cflags);
            merge(flags, compilation_flags.iflags);
            merge(flags, compilation_flags.defines);
            merge(flags, compilation_flags.platform_cflags);
            merge(flags, compilation_flags.compiler_cflags);

            compilation_unit_type = compilation_unit_type_;
        }

        ArchiveTarget::ArchiveTarget(const std::string lib, const std::vector<std::string> objs, const std::vector<std::string> aflags) :
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

        LinkTarget::LinkTarget(const std::string exe, const std::vector<std::string> objs, const std::vector<std::string> libs, const std::vector<std::string> lflags, const BuildLinkType link_type_) :
            BuildTarget(exe, exe),
            link_type(link_type_) {
            inputs = objs;
            inputs.insert(inputs.end(), libs.begin(), libs.end());
            flags = lflags;
        }
    } // namespace build
} // namespace muuk