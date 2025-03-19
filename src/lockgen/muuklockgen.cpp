#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glob/glob.h>
#include <tl/expected.hpp>
#include <toml++/toml.h>

#include "buildconfig.h"
#include "logger.h"
#include "muuk.h"
#include "muukfiler.h"
#include "muuklockgen.h"
#include "rustify.hpp"
#include "util.h"

namespace fs = std::filesystem;

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) :
    base_path_(base_path) {
    muuk::logger::trace("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages = {};
}

// Parse a single muuk.toml file representing a package
void MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
    muuk::logger::trace("Attempting to parse muuk.toml: {}", path);
    if (!fs::exists(path)) {
        muuk::logger::error("Failed to locate 'muuk.toml' at path '{}'. Check if the file exists.", path);
        return;
    }

    auto muuk_filer = MuukFiler::create(path);
    if (!muuk_filer) {
        muuk::logger::error("Failed to create MuukFiler for '{}'", path);
        return;
    }

    auto data = muuk_filer->get_config();

    if (!data.contains("package") || !data["package"].is_table()) {
        muuk::logger::error("Missing 'package' section in TOML file.");
        return;
    }

    const std::string package_name = data["package"]["name"].value_or<std::string>("unknown");

    const auto package_version_result = data["package"]["version"].value<std::string>();
    if (!package_version_result) {
        muuk::logger::warn("Version not specified for base package '{}'.", package_name);
        return;
    }
    const auto& package_version = package_version_result.value_or("0.0.0");

    muuk::logger::info("Parsing muuk.toml for package: {} ({})", package_name, package_version);

    muuk::logger::info(
        "Parsing package: {} (version: {}) with muuk path: `{}`",
        package_name,
        package_version,
        path);

    auto package = std::make_shared<Package>(
        package_name,
        package_version,
        fs::path(path).parent_path().string(),
        "library");

    Try(parse_dependencies(data, package));
    if (data.contains("library") && data["library"].is_table())
        Try(parse_library(*data["library"].as_table(), package));
    parse_platform(data, package);
    parse_compiler(data, package);
    parse_features(data, package);

    resolved_packages[package_name][package_version] = package;

    if (is_base) {

        parse_profile(data);
        parse_builds(data, package_version, path);
        base_package_ = package;

    } // if (is_base)
}

tl::expected<void, std::string> MuukLockGenerator::resolve_dependencies(const std::string& package_name, std::optional<std::string> version, std::optional<std::string> search_path) {
    if (visited.count(package_name)) {
        muuk::logger::warn("Circular dependency detected for '{}'. Skipping resolution.", package_name);
        return {};
    }

    visited.insert(package_name);
    muuk::logger::info("Resolving dependencies for: {} with muuk path: '{}'", package_name, search_path.value_or(""));

    std::optional<std::shared_ptr<Package>> package_opt = find_package(package_name, version);

    if (!package_opt.has_value()) {
        if (search_path) {
            fs::path search_file = fs::path(search_path.value());
            if (!search_path.value().ends_with("muuk.toml")) {
                muuk::logger::info("Search path '{}' does not end with 'muuk.toml', appending it.", search_file.string());
                search_file /= "muuk.toml";
            } else {
                muuk::logger::info("Search path '{}' already ends with 'muuk.toml', using as is.", search_file.string());
            }

            if (fs::exists(search_file)) {
                parse_muuk_toml(search_file.string());
                package_opt = resolved_packages[package_name][version.value_or("")];

                if (version.has_value()) {
                    package_opt = resolved_packages[package_name][version.value()];
                } else {
                    muuk::logger::error("Version not specified for package '" + package_name + "'.");
                    return Err("Version not specified for package '" + package_name + "'.");
                }

                if (!package_opt) {
                    muuk::logger::error("Package '{}' not found after parsing '{}'.", package_name, *search_path);
                    return Err("");
                }
            } else {
                muuk::logger::error("Search file '{}' for '{}' does not exist.", search_file.string(), package_name);
                return Err("");
            }
        } else {
            // If no search path, search in dependency folders
            search_and_parse_dependency(package_name, version.value());
            package_opt = resolved_packages[package_name][version.value()];

            if (!package_opt.has_value()) {
                muuk::logger::error("Package '{}' not found after searching the dependency folder ({}).", package_name, DEPENDENCY_FOLDER);
                return Err("");
            }
        }
        // muuk::logger::info("Package '{}' not found in resolved packages.", package_name);
    }

    auto package = package_opt.value();
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
                resolve_system_dependency(dep_name, package);
            } else {
                // Resolve dependency, passing the search path if available
                auto result = resolve_dependencies(
                    dep_name,
                    dep_version,
                    dep_search_path.empty()
                        ? std::nullopt
                        : std::optional<std::string> { dep_search_path });

                if (!result) {
                    std::string error_msg = "Failed to resolve dependency '" + dep_name + "' for package '" + package_name + "'.";
                    muuk::logger::error(error_msg);
                    return Err("");
                }
            }

            if (
                resolved_packages.count(dep_name) && resolved_packages[dep_name].count(dep_version)) {
                muuk::logger::info("Merging '{}' into '{}'", dep_name, package_name);
                resolved_deps.emplace_back(dep_name, dep_version, dep_info);
            }
        }
    }

    muuk::logger::info("Added '{}' to resolved order list.", package_name);
    resolved_order_.push_back(std::make_pair(package_name, version.value_or("unknown")));
    return {};
}

Result<void> MuukLockGenerator::merge_resolved_dependencies(const std::string& package_name, std::optional<std::string> version) {
    std::optional<std::shared_ptr<Package>> package_opt = find_package(package_name, version);

    if (!package_opt) {
        muuk::logger::error("Package '{}' not found.", package_name);
        return {};
    }

    auto package = package_opt.value();

    // Base case: If no dependencies, return
    if (package->dependencies_.empty()) {
        return {};
    }

    muuk::logger::info("Merging dependencies into '{}'", package_name);

    for (const auto& [dep_name, version_map] : package->dependencies_) {
        for (const auto& [dep_version, dep_info] : version_map) {
            if (resolved_packages.count(dep_name) && resolved_packages[dep_name].count(dep_version)) {
                auto dep_package = resolved_packages[dep_name][dep_version];

                // Recursively merge dependencies for this package first
                merge_resolved_dependencies(dep_name, dep_version);

                // Now merge this dependency into the current package
                muuk::logger::info("Merging '{}' into '{}'", dep_name, package_name);
                package->merge(*dep_package);
            }
        }
    }

    return {};
}

// Searches for the specified package in the dependency folder and parses its muuk.toml file if found.
void MuukLockGenerator::search_and_parse_dependency(const std::string& package_name, const std::string& version) {
    muuk::logger::info("Searching for target package '{}', version '{}'.", package_name, version);
    fs::path search_dir = fs::path(DEPENDENCY_FOLDER) / package_name / version;

    // TODO: Make this return matter
    if (!fs::exists(search_dir)) {
        muuk::logger::warn("Dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
        return;
    }

    fs::path dep_path = search_dir / "muuk.toml";
    if (fs::exists(dep_path)) {
        parse_muuk_toml(dep_path.string());
    } else {
        muuk::logger::warn("muuk.toml for dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
    }
}

// Generates the muuk.lock.toml file by parsing the base muuk.toml, resolving dependencies,
// and writing the resolved packages and profiles to the lockfile.
void MuukLockGenerator::generate_lockfile(const std::string& output_path) {
    // muuk::logger::info("------------------------------");
    muuk::logger::info("");
    muuk::logger::info(" Generating muuk.lock.toml...");
    muuk::logger::info("------------------------------");

    std::ofstream lockfile(output_path);
    if (!lockfile) {
        muuk::logger::error("Failed to open lockfile: {}", output_path);
        return;
    }

    parse_muuk_toml(base_path_ + "muuk.toml", true);

    // TODO Take directly from `parse_muuk_toml`
    // Extract the package name and version from the base muuk.toml
    auto base_muuk_filer = MuukFiler::create(base_path_ + "muuk.toml");
    if (!base_muuk_filer) {
        muuk::logger::error("Failed to create MuukFiler for base muuk.toml");
        return;
    }

    auto base_data = base_muuk_filer->get_config();
    if (!base_data.contains("package") || !base_data["package"].is_table()) {
        muuk::logger::error("Missing 'package' section in base muuk.toml file.");
        return;
    }

    // TODO: Return Err if unknown
    const std::string base_package_name = base_data["package"]["name"].value_or<std::string>("unknown");
    const std::string base_package_version = base_data["package"]["version"].value_or<std::string>("0.0.0");
    muuk::logger::info("Base package name extracted: {}, version: {}", base_package_name, base_package_version);

    const auto& base_package_dep = Dependency::from_toml(base_data);

    // Resolve dependencies for the base package
    resolve_dependencies(base_package_name, base_package_version);

    muuk::logger::info("Resolving dependencies for build packages...");
    for (const auto& [build_name, build] : builds) {
        auto result = resolve_dependencies(build_name);
        if (!result) {
            muuk::logger::error(
                "Failed to resolve dependencies for build package '{}': {}",
                build_name,
                result.error());
        }
    }

    for (const auto& [package_name, version] : resolved_order_) {
        auto package_opt = find_package(package_name, version);
        if (!package_opt)
            continue;

        auto package = package_opt.value();

        for (const auto& [dep_name, version_map] : package->dependencies_) {
            for (const auto& [dep_version, dep_info] : version_map) {
                if (dependencies_.count(dep_name) && dependencies_[dep_name].count(dep_version)) {
                    auto dep = dependencies_[dep_name][dep_version];
                    auto package_dep = resolved_packages[dep_name][dep_version];
                    package_dep->enable_features(dep->enabled_features);
                }
            }
        }
    }

    merge_resolved_dependencies(base_package_name, base_package_version);

    for (const auto& [build_name, build] : builds) {
        auto result = merge_resolved_dependencies(build_name);
        if (!result) {
            muuk::logger::error(
                "Failed to merge dependencies for build package '{}': {}",
                build_name,
                result.error());
        }

        build->deps.insert(base_package_name);
        build->deps_all_[base_package_name][base_package_version] = std::make_shared<Dependency>(base_package_dep);
        build->merge(*base_package_);
    }

    for (const auto& [package_name, version] : resolved_order_) {
        const auto package_opt = find_package(package_name, version);
        if (!package_opt)
            continue;

        const auto package = package_opt.value();
        const std::string package_type = package->package_type;

        const std::string package_table = package->serialize();

        lockfile << "[[" << package_type << "]]\nname = \"" << package_name << "\"\n";
        lockfile << package_table << "\n";

        muuk::logger::info("Written package '{}' to lockfile.", package_name);
    }

    lockfile << format_dependencies(dependencies_);

    if (!profiles_.empty()) {
        for (const auto& [profile_name, profile_settings] : profiles_) {
            lockfile << "[profile." << profile_name << "]\n";

            for (const auto& [setting_name, values] : profile_settings) {
                toml::array value_array;
                for (const auto& value : values) {
                    value_array.push_back(value);
                }
                lockfile << setting_name << " = " << value_array << "\n";
            }

            lockfile << "\n";
        }
    }

    lockfile.close();

    muuk::logger::info("muuk.lock.toml generation complete!");
}

// Finds and returns a package by its name from the resolved packages.
std::optional<std::shared_ptr<Package>> MuukLockGenerator::find_package(const std::string& package_name, std::optional<std::string> version) {
    if (resolved_packages.count(package_name) && version.has_value()) {
        return resolved_packages[package_name][version.value()];
    }

    if (builds.count(package_name)) {
        return builds[package_name];
    }
    return std::nullopt;
}

// Resolves system dependencies by checking for include and library paths for the given package name.
void MuukLockGenerator::resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Package>> package) {
    muuk::logger::info("Checking system dependency: '{}'", package_name);

    std::string include_path, lib_path;

    // Special handling for Python
    if (package_name == "python" || package_name == "python3") {
#ifdef _WIN32
        include_path = util::command_line::execute_command_get_out("python -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::command_line::execute_command_get_out("python -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#else
        include_path = util::command_line::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::command_line::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#endif
    }
    // Special handling for Boost
    else if (package_name == "boost") {
#ifdef _WIN32
        include_path = util::command_line::execute_command_get_out("where boostdep");
        lib_path = util::command_line::execute_command_get_out("where boost");
#else
        include_path = util::command_line::execute_command("boostdep --include-path");
        lib_path = util::command_line::execute_command("boostdep --lib-path");
#endif
    } else {
#ifdef _WIN32
        muuk::logger::warn("System dependency '{}' resolution on Windows is not fully automated. Consider setting paths manually.", package_name);
#else
        include_path = util::command_line::execute_command("pkg-config --cflags-only-I " + package_name + " | sed 's/-I//' | tr -d '\n'");
        lib_path = util::command_line::execute_command("pkg-config --libs-only-L " + package_name + " | sed 's/-L//' | tr -d '\n'");
#endif
    }

    if (!include_path.empty() && util::path_exists(include_path)) {
        system_include_paths_.insert(include_path);
        if (package)
            (*package)->add_include_path(include_path);
        muuk::logger::info("  - Found Include Path: {}", include_path);
    } else {
        muuk::logger::warn("  - Include path for '{}' not found.", package_name);
    }

    if (!lib_path.empty() && fs::exists(lib_path)) {
        system_library_paths_.insert(lib_path);
        if (package)
            (*package)->libs.push_back(lib_path);
        muuk::logger::info("  - Found Library Path: {}", lib_path);
    } else {
        muuk::logger::warn("  - Library path for '{}' not found.", package_name);
    }

    if (include_path.empty() && lib_path.empty()) {
        muuk::logger::error("Failed to resolve system dependency '{}'. Consider installing it or specifying paths manually.", package_name);
    }
}

// Merges the settings of the inherited profile into the base profile.
void MuukLockGenerator::merge_profiles(const std::string& base_profile, const std::string& inherited_profile) {
    if (profiles_.find(inherited_profile) == profiles_.end()) {
        muuk::logger::error("Inherited profile '{}' not found with base profile {}.", inherited_profile, base_profile);
        return;
    }

    muuk::logger::trace("Merging profile '{}' into '{}'", inherited_profile, base_profile);

    for (const auto& [key, values] : profiles_[inherited_profile]) {
        profiles_[base_profile][key].insert(values.begin(), values.end());
    }
}

std::string MuukLockGenerator::format_dependencies(const DependencyVersionMap<toml::table>& dependencies, std::string section_name) {
    std::ostringstream oss;
    oss << "[" << section_name << "]\n";

    for (const auto& [dep_name, versions] : dependencies) {
        for (const auto& [version, dep_info] : versions) {
            oss << dep_name << " = { ";

            std::string dep_entries;
            bool first = true;

            for (const auto& [key, val] : dep_info) {
                if (!first)
                    dep_entries += ", ";
                dep_entries += fmt::format("{} = '{}'", std::string(key.str()), *val.value<std::string>());
                first = false;
            }

            oss << fmt::format("{} }}\n", dep_entries);
        }
    }

    oss << "\n";
    return oss.str();
}

std::string MuukLockGenerator::format_dependencies(
    const DependencyVersionMap<std::shared_ptr<Dependency>>& dependencies,
    const std::string& section_name) {
    std::ostringstream oss;
    oss << "[" << section_name << "]\n";

    for (const auto& [dep_name, versions] : dependencies) {
        for (const auto& [version, dep_ptr] : versions) {
            if (!dep_ptr) {
                muuk::logger::warn("Skipping dependency '{}' (version '{}') due to null pointer.", dep_name, version);
                continue;
            }

            const toml::table dep_table = dep_ptr->to_toml(); // Convert Dependency to TOML table

            oss << dep_name << " = { ";

            std::vector<std::string> dep_entries;
            for (const auto& [key, val] : dep_table) {
                std::string key_str = std::string(key.str());

                if (val.is_string()) {
                    dep_entries.push_back(fmt::format("{} = '{}'", key_str, *val.value<std::string>()));
                } else if (val.is_array()) {
                    std::vector<std::string> array_values;
                    for (const auto& item : *val.as_array()) {
                        if (item.is_string()) {
                            array_values.push_back(fmt::format("'{}'", *item.value<std::string>()));
                        }
                    }
                    dep_entries.push_back(fmt::format("{} = [{}]", key_str, fmt::join(array_values, ", ")));
                } else {
                    muuk::logger::warn("Skipping unsupported TOML type for key '{}'", key_str);
                }
            }

            oss << fmt::format("{} }}\n", fmt::join(dep_entries, ", "));
        }
    }

    oss << "\n";
    return oss.str();
}

void MuukLockGenerator::parse_and_store_flag_categories(
    const toml::table& data,
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>& target,
    const std::vector<std::string>& flag_categories) {
    for (const auto& [category_name, category_data] : data) {
        if (!category_data.is_table()) {
            continue;
        }

        const std::string category_name_str = std::string(category_name.str());

        for (const auto& flag_type : flag_categories) {
            target[category_name_str][flag_type] = MuukFiler::parse_array_as_set(
                *category_data.as_table(), flag_type);
        }
    }
}
