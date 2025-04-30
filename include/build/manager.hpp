#ifndef BUILD_MANAGER_H
#define BUILD_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "build/targets.hpp"

struct BuildProfile {
    std::vector<std::string> cflags;
    std::vector<std::string> lflags;
    std::vector<std::string> defines;
};

class BuildManager {
    std::vector<CompilationTarget> compilation_targets;
    std::vector<ArchiveTarget> archive_targets;
    std::vector<LinkTarget> link_targets;
    std::unordered_map<std::string, std::string> object_registry;
    std::unordered_map<std::string, std::string> library_registry;

    std::unordered_map<std::string, BuildProfile> profiles;

public:
    void add_compilation_target(
        const std::string src,
        const std::string obj,
        const CompilationFlags compilation_flags,
        const CompilationUnitType compilation_unit_type = CompilationUnitType::Source);

    void add_archive_target(
        const std::string lib,
        const std::vector<std::string> objs,
        const std::vector<std::string> aflags);

    void add_link_target(
        const std::string exe,
        const std::vector<std::string> objs,
        const std::vector<std::string> libs,
        const std::vector<std::string> lflags);

    std::vector<CompilationTarget>& get_compilation_targets();
    const std::vector<CompilationTarget>& get_compilation_targets() const;

    std::vector<ArchiveTarget>& get_archive_targets();
    const std::vector<ArchiveTarget>& get_archive_targets() const;

    std::vector<LinkTarget>& get_link_targets();
    const std::vector<LinkTarget>& get_link_targets() const;

    CompilationTarget* find_compilation_target(
        const std::string& key,
        const std::string& value);

    void set_profile_flags(
        const std::string& profile_name,
        const BuildProfile profile);

    const BuildProfile* get_profile(const std::string& profile_name) const;
};

#endif