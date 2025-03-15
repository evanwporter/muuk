#include <iostream>

#include "buildmanager.h"
#include "buildtargets.h"
#include "logger.h"

void BuildManager::add_compilation_target(std::string src, std::string obj, std::vector<std::string> cflags, std::vector<std::string> iflags) {
    // Prevent empty inputs
    if (src.empty() || obj.empty()) {
        std::cerr << "Error: Compilation target must have a source file and an object file.\n";
        return;
    }

    if (object_registry.find(obj) == object_registry.end()) {
        compilation_targets.emplace_back(src, obj, cflags, iflags);
        object_registry[obj] = obj;
    }
}

void BuildManager::add_archive_target(std::string lib, std::vector<std::string> objs, std::vector<std::string> aflags) {
    if (lib.empty() || objs.empty()) {
        muuk::logger::trace("Skipping since Archive target must have a library name and at least one object file.\n");
        return;
    }
    if (library_registry.find(lib) == library_registry.end()) {
        archive_targets.emplace_back(lib, objs, aflags);
        library_registry[lib] = lib;
    }
}

void BuildManager::add_link_target(std::string exe, std::vector<std::string> objs, std::vector<std::string> libs, std::vector<std::string> lflags) {
    if (exe.empty() || objs.empty()) {
        std::cerr << "Error: Link target must have an executable name and at least one object file.\n";
        return;
    }

    link_targets.emplace_back(exe, objs, libs, lflags);
}

std::vector<CompilationTarget> BuildManager::get_compilation_targets() const {
    return compilation_targets;
}

std::vector<ArchiveTarget> BuildManager::get_archive_targets() const {
    return archive_targets;
}

std::vector<LinkTarget> BuildManager::get_link_targets() const {
    return link_targets;
}

CompilationTarget* BuildManager::find_compilation_target(const std::string& key, const std::string& value) {
    auto it = std::find_if(
        compilation_targets.begin(),
        compilation_targets.end(),
        [&](const CompilationTarget& target) {
            return (key == "input" && target.input == value) || (key == "output" && target.output == value);
        });

    return (it != compilation_targets.end()) ? &(*it) : nullptr;
}