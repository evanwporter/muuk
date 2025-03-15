#pragma once
#ifndef NINJA_ENTRIES_H
#define NINJA_ENTRIES_H

#include <string>
#include <vector>

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
    CompilationTarget(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags);
    virtual ~CompilationTarget() = default;

    std::string input;
    std::string logical_name;
    std::vector<CompilationTarget*> dependencies; // Files that must be built first
};

class ArchiveTarget : public BuildTarget {
public:
    ArchiveTarget(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags);
    virtual ~ArchiveTarget() = default;
};

class LinkTarget : public BuildTarget {
public:
    LinkTarget(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags);
    virtual ~LinkTarget() = default;
};

#endif