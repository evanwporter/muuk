#include <vector>

#include "build/manager.hpp"
#include "build/targets.hpp"
#include "compiler.hpp"
#include "logger.hpp"

namespace muuk {
    namespace build {
        void BuildManager::add_compilation_target(
            const std::string src,
            const std::string obj,
            const CompilationFlags compilation_flags,
            const CompilationUnitType compilation_unit_type) {

            // Prevent empty inputs
            if (src.empty() || obj.empty()) {
                muuk::logger::error("Compilation target must have a source file and an object file.");
                return;
            }

            if (object_registry.find(obj) == object_registry.end()) {
                compilation_targets.emplace_back(src, obj, compilation_flags, compilation_unit_type);
                object_registry[obj] = obj;
            }
        }

        void BuildManager::add_archive_target(const std::string lib, const std::vector<std::string> objs, const std::vector<std::string> aflags) {
            if (lib.empty() || objs.empty()) {
                muuk::logger::trace("Skipping since Archive target must have a library name and at least one object file.\n");
                return;
            }
            if (library_registry.find(lib) == library_registry.end()) {
                archive_targets.emplace_back(lib, objs, aflags);
                library_registry[lib] = lib;
            }
        }

        void BuildManager::add_external_target(const std::string& type_, const std::vector<std::string>& outputs, const std::string& build_path, const std::string& source_path, const std::string& source_file, const std::string& cache_file) {
            external_targets.emplace_back(
                type_,
                outputs,
                build_path,
                source_path,
                source_file,
                cache_file);
        }

        void BuildManager::add_link_target(const std::string exe, const std::vector<std::string> objs, const std::vector<std::string> libs, const std::vector<std::string> lflags, const BuildLinkType link_type) {
            if (exe.empty() || objs.empty()) {
                // TODO: It can have no object files
                muuk::logger::error("Link target must have an executable name and at least one object file.");
                return;
            }

            link_targets.emplace_back(exe, objs, libs, lflags, link_type);
        }

        std::vector<CompilationTarget>& BuildManager::get_compilation_targets() {
            return compilation_targets;
        }

        const std::vector<CompilationTarget>& BuildManager::get_compilation_targets() const {
            return compilation_targets;
        }

        std::vector<ArchiveTarget>& BuildManager::get_archive_targets() {
            return archive_targets;
        }

        const std::vector<ArchiveTarget>& BuildManager::get_archive_targets() const {
            return archive_targets;
        }

        std::vector<ExternalTarget>& BuildManager::get_external_targets() {
            return external_targets;
        }

        const std::vector<ExternalTarget>& BuildManager::get_external_targets() const {
            return external_targets;
        }

        std::vector<LinkTarget>& BuildManager::get_link_targets() {
            return link_targets;
        }

        const std::vector<LinkTarget>& BuildManager::get_link_targets() const {
            return link_targets;
        }

        CompilationTarget* BuildManager::find_compilation_target(const std::string& key, const std::string& value) {
            auto it = std::find_if(
                compilation_targets.begin(),
                compilation_targets.end(),
                [&](const CompilationTarget& target) {
                    return (key == "input" && target.input == value) || (key == "output" && target.output == value);
                });

            return (it != compilation_targets.end())
                ? &(*it)
                : nullptr;
        }

        void BuildManager::set_profile_flags(const std::string& profile_name, const BuildProfile profile) {
            profiles[profile_name] = std::move(profile);
        }

        const BuildProfile* BuildManager::get_profile(const std::string& profile_name) const {
            auto it = profiles.find(profile_name);
            if (it != profiles.end()) {
                return &it->second;
            }
            return nullptr;
        }

    } // namespace build
} // namespace muuk