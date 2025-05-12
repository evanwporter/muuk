#pragma once
#include "compiler.hpp"
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
            /// Unique target name/path (e.g., obj file, archive, executable)
            std::string name;

            /// Input files (source files)
            std::vector<std::string> inputs;

            /// Output file (e.g., .o, .a, executable)
            std::string output;

            /// Compiler, linker, or archive flags
            std::vector<std::string> flags;

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
            ArchiveTarget(
                std::string lib,
                std::vector<std::string> objs,
                std::vector<std::string> aflags);
            virtual ~ArchiveTarget() = default;
        };

        class ExternalTarget : public BuildTarget {
        public:
            /// "cmake", "make", etc.
            std::string type;

            /// Path to the external project
            std::string build_path;

            std::vector<std::string> args; // Build configuration arguments

            /// List of produced artifacts (e.g. .a, .so, .dll)
            std::vector<std::string> outputs;

            /// Path to the source directory.
            std::string source_path;

            /// Path to the source file. Treated as an input file.
            /// (eg. CMakeLists.txt)
            std::string source_file;

            /// Cache file path.
            /// ie: CMakeCache.txt.
            /// This is used to check if the external project has already been configured.
            std::string cache_file;

            ExternalTarget(
                const std::string& type_,
                const std::vector<std::string>& outputs_,
                const std::string& build_path_,
                const std::string& source_path_,
                const std::string& source_file_,
                const std::string& cache_file_);

            virtual ~ExternalTarget() = default;
        };

        class LinkTarget : public BuildTarget {
        public:
            LinkTarget(
                const std::string exe,
                const std::vector<std::string> objs,
                const std::vector<std::string> libs,
                const std::vector<std::string> lflags,
                const BuildLinkType link_type_);

            const BuildLinkType link_type;

            virtual ~LinkTarget() = default;
        };

    } // namespace build
} // namespace muuk
#endif