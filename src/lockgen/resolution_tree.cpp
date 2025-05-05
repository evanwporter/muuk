#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "buildconfig.h"
#include "error_codes.hpp"
#include "lockgen/muuklockgen.hpp"
#include "logger.hpp"
#include "rustify.hpp"

namespace fs = std::filesystem;

Result<void> MuukLockGenerator::locate_and_parse_package(const std::string& package_name, const std::optional<std::string> version, std::shared_ptr<Package>& package, const std::optional<std::string> search_path) {
    if (search_path) {
        fs::path search_file = fs::path(search_path.value());
        if (!search_path.value().ends_with("muuk.toml")) {
            muuk::logger::info("Search path '{}' does not end with 'muuk.toml', appending it.", search_file.string());
            search_file /= "muuk.toml";
        } else {
            muuk::logger::info("Search path '{}' already ends with 'muuk.toml', using as is.", search_file.string());
        }

        if (fs::exists(search_file)) {
            auto result = parse_muuk_toml(search_file.string());
            if (!result) {
                return Err(result);
            };

            package = resolved_packages[package_name][version.value_or("")];

            if (version.has_value())
                package = resolved_packages[package_name][version.value()];
            else
                return Err("Version not specified for package '" + package_name + "'.");

            if (!package)
                return Err("Package '{}' not found after parsing '{}'.", package_name, *search_path);
        } else {
            return muuk::make_error<muuk::EC::FileNotFound>(search_file.string());
        }
    } else {
        // If no search path, search in dependency folders
        TRYV(search_and_parse_dependency(package_name, version.value()));

        package = find_package(package_name, version.value());

        if (!package)
            return Err("Package '{}' not found after searching the dependency folder ({}).", package_name, DEPENDENCY_FOLDER);
    }

    return {};
}

Result<void> MuukLockGenerator::resolve_dependencies(const std::string& package_name, std::optional<std::string> version, std::optional<std::string> search_path) {
    if (visited.count(package_name)) {
        muuk::logger::trace("Dependency '{}' already processed. Skipping resolution.", package_name);
        return {};
    }

    // TODO: ALso account for visited version
    visited.insert(package_name);
    muuk::logger::info("Resolving dependencies for: {} with muuk path: '{}'", package_name, search_path.value_or(""));

    auto package = find_package(package_name, version);

    // If not found attempt to locate and parse
    if (!package) {
        TRYV(locate_and_parse_package(package_name, version, package, search_path));
        // muuk::logger::info("Package '{}' not found in resolved packages.", package_name);
    }

    std::vector<std::tuple<std::string, std::string, std::shared_ptr<Dependency>>> resolved_deps;

    for (const auto& [dep_name, version_map] : package->dependencies_) {
        for (const auto& [dep_version, dep_info] : version_map) {

            if (dep_name == package_name) {
                muuk::logger::warn("Circular dependency detected: '{}' depends on itself. Skipping.", package_name);
                continue;
            }

            muuk::logger::info("Resolving dependency '{}' for '{}'", dep_name, package_name);

            std::string dep_search_path;
            if (!dep_info->path.empty()) {
                dep_search_path = dep_info->path;
                muuk::logger::info("Using defined muuk_path for dependency '{}': {}", dep_name, dep_search_path);
            }

            if (dep_info->system) {
                // TODO: PASS ENABLED LIBS FOR SYSTEM DEPS
                //       GET DEP INFO FROM `PACKAGE` DEPENDENCIES W/ DEP_NAME
                //       ADD IT TO THE PACKAGES LIBS AND SHIZZ
                resolve_system_dependency(dep_name, package);
            } else {
                // Resolve dependency, passing the search path if available
                // This will parse the search path and stuff
                TRYV(resolve_dependencies(
                    dep_name,
                    dep_version,
                    dep_search_path.empty()
                        ? std::nullopt
                        : std::optional<std::string> { dep_search_path }));
            }

            if (resolved_packages.count(dep_name) && resolved_packages[dep_name].count(dep_version)) {
                muuk::logger::info("Merging '{}' into '{}'", dep_name, package_name);
                resolved_deps.emplace_back(dep_name, dep_version, dep_info);
            }
        }
    }

    muuk::logger::info("Added '{}' to resolved order list.", package_name);
    resolved_order_.push_back(std::make_pair(package_name, version.value_or("unknown")));
    return {};
}

// TODO: This need to be redone
void MuukLockGenerator::resolve_system_dependency(const std::string& package_name, std::shared_ptr<Package> package) {
    //     muuk::logger::info("Resolving system dependency: '{}'", package_name);

    //     std::string include_path, lib_path;

    //     // Attempt to find the dependency in the main dependency map
    //     std::shared_ptr<Dependency> dep_info;
    //     if (dependencies_.count(package_name)) {
    //         for (const auto& [_, ptr] : dependencies_[package_name]) {
    //             if (ptr) {
    //                 dep_info = ptr;
    //                 break;
    //             }
    //         }
    //     }

    //     std::optional<std::string> custom_path;
    //     if (dep_info && !dep_info->path.empty()) {
    //         custom_path = dep_info->path;
    //         muuk::logger::info("Using custom path '{}' for system dependency '{}'", *custom_path, package_name);
    //     }

    //     // Check custom path
    //     if (custom_path && fs::exists(*custom_path)) {
    //         muuk::logger::info("Checking custom path for '{}': {}", package_name, *custom_path);

    //         // Search for include and lib manually under path
    //         fs::path inc_dir = fs::path(*custom_path) / "include";
    //         fs::path lib_dir = fs::path(*custom_path) / "lib";

    //         if (fs::exists(inc_dir))
    //             include_path = inc_dir.string();
    //         if (fs::exists(lib_dir))
    //             lib_path = lib_dir.string();
    //     }

    //     // Fallback to pkg-config or system tools
    //     if (include_path.empty() || lib_path.empty()) {
    // #ifdef _WIN32
    //         muuk::logger::warn("System dependency '{}' resolution on Windows is limited. Ensure proper path is provided.", package_name);
    // #else
    //         muuk::logger::info("Using pkg-config for '{}'", package_name);
    //         include_path = util::command_line::execute_command("pkg-config --cflags-only-I " + package_name + " | sed 's/-I//' | tr -d '\n'");
    //         lib_path = util::command_line::execute_command("pkg-config --libs-only-L " + package_name + " | sed 's/-L//' | tr -d '\n'");
    // #endif
    //     }

    //     // Save resolved paths
    //     if (!include_path.empty() && util::file_system::path_exists(include_path)) {
    //         system_include_paths_.insert(include_path);
    //         if (package)
    //             package->add_include_path(include_path);
    //         muuk::logger::info("  - Resolved Include Path: {}", include_path);
    //     } else {
    //         muuk::logger::warn("  - Include path for '{}' not found.", package_name);
    //     }

    //     if (!lib_path.empty() && fs::exists(lib_path)) {
    //         system_library_paths_.insert(lib_path);
    //         if (package)
    //             package->libs.push_back(lib_path);
    //         muuk::logger::info("  - Resolved Library Path: {}", lib_path);
    //     } else {
    //         muuk::logger::warn("  - Library path for '{}' not found.", package_name);
    //     }

    //     // Add specified libs to package
    //     if (package && dep_info && !dep_info->libs.empty()) {
    //         muuk::logger::info("  - Linking specified libs for '{}': {}", package_name, fmt::join(dep_info->libs, ", "));
    //         for (const auto& lib : dep_info->libs) {
    //             package->libs.push_back(lib);
    //         }
    //     }

    //     if (include_path.empty() && lib_path.empty() && (!dep_info || dep_info->libs.empty())) {
    //         muuk::logger::error("Failed to resolve system dependency '{}'. Provide a valid path or ensure it is installed.", package_name);
    //     }
}