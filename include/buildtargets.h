#pragma once
#ifndef NINJA_ENTRIES_H
#define NINJA_ENTRIES_H

#include "../include/logger.h"
#include "../include/muuk.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>

class BuildTarget {
public:
    std::string name;                   // Unique target name (e.g., obj file, archive, executable)
    std::vector<std::string> inputs;    // Input files (source files or dependencies)
    std::string output;                 // Output file (e.g., .o, .a, executable)
    std::vector<std::string> flags;     // Compiler, linker, or archive flags
    std::vector<std::string> dependencies; // Files that must be built first

    BuildTarget(std::string target_name, std::string target_output);

    virtual std::string generate_ninja_rule() const = 0;
};

class CompilationTarget : public BuildTarget {
public:
    CompilationTarget(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags);

    std::string generate_ninja_rule() const override;
};

class ArchiveTarget : public BuildTarget {
public:
    ArchiveTarget(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags);

    std::string generate_ninja_rule() const override;
};

class LinkTarget : public BuildTarget {
public:
    LinkTarget(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags);

    std::string generate_ninja_rule() const override;
};

#endif