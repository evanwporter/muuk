#pragma once
#ifndef NINJA_ENTRIES_H
#define NINJA_ENTRIES_H

#include <string>
#include <vector>

namespace muuk {
    namespace build {
        enum class CompilationUnitType {
            Module,
            Source,
            Count
        };

        struct CompilationFlags {
            std::vector<std::string> cflags;
            std::vector<std::string> iflags;
            std::vector<std::string> defines;
            std::vector<std::string> platform_cflags;
            std::vector<std::string> compiler_cflags;
        };

        class BuildTarget {
        public:
            std::string name; // Unique target name (e.g., obj file, archive, executable)
            std::vector<std::string> inputs; // Input files (source files or dependencies)
            std::string output; // Output file (e.g., .o, .a, executable)
            std::vector<std::string> flags; // Compiler, linker, or archive flags

            BuildTarget(std::string target_name, std::string target_output);
            virtual ~BuildTarget() = default;
        };

        class CompilationTarget : public BuildTarget {
        public:
            CompilationTarget(
                std::string src,
                std::string obj,
                CompilationFlags compilation_flags,
                CompilationUnitType comp_type = CompilationUnitType::Source);

            virtual ~CompilationTarget() = default;

            std::string input;
            std::string logical_name;
            std::vector<CompilationTarget*> dependencies; // Files that must be built first

            /// Indicates whether the target is a module or a source file.
            CompilationUnitType compilation_unit_type;
        };

        class ArchiveTarget : public BuildTarget {
        public:
            ArchiveTarget(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags);
            virtual ~ArchiveTarget() = default;
        };

        class ExternalTarget : public BuildTarget {
        public:
            std::string version;
            std::string type; // "cmake", "make", etc.
            std::string path; // Path to the external project
            std::vector<std::string> args; // Build configuration arguments
            std::vector<std::string> outputs; // List of produced artifacts (e.g. .a, .so, .dll)

            ExternalTarget(
                const std::string& name,
                const std::string& version,
                const std::string& type,
                const std::string& path,
                const std::vector<std::string>& args,
                std::vector<std::string>& outputs);

            virtual ~ExternalTarget() = default;
        };

        class LinkTarget : public BuildTarget {
        public:
            LinkTarget(
                std::string exe,
                std::vector<std::string> objs,
                std::vector<std::string> libs,
                std::vector<std::string> lflags);
            virtual ~LinkTarget() = default;
        };

    } // namespace build
} // namespace muuk
#endif