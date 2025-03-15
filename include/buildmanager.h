#ifndef BUILD_MANAGER_H
#define BUILD_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "buildtargets.h"

class BuildManager {
    std::vector<CompilationTarget> compilation_targets;
    std::vector<ArchiveTarget> archive_targets;
    std::vector<LinkTarget> link_targets;
    std::unordered_map<std::string, std::string> object_registry;
    std::unordered_map<std::string, std::string> library_registry;

public:
    void add_compilation_target(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags);
    void add_archive_target(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags);
    void add_link_target(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags);

    std::vector<CompilationTarget> get_compilation_targets() const;
    std::vector<ArchiveTarget> get_archive_targets() const;
    std::vector<LinkTarget> get_link_targets() const;

    CompilationTarget* find_compilation_target(const std::string& key, const std::string& value);
};

#endif