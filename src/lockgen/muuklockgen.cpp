#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glob/glob.hpp>
#include <tl/expected.hpp>
#include <toml.hpp>

#include "buildconfig.h"
#include "compiler.hpp"
#include "error_codes.hpp"
#include "logger.h"
#include "muuk.h"
#include "muuk_parser.hpp"
#include "muuklockgen.h"
#include "rustify.hpp"

namespace fs = std::filesystem;

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) :
    base_path_(base_path) {
    muuk::logger::trace("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages = {};
}

Result<MuukLockGenerator> MuukLockGenerator::create(const std::string& base_path) {
    auto lockgen = MuukLockGenerator(base_path);

    auto result = lockgen.load();
    if (!result) {
        return Err(result);
    }

    return lockgen;
}

// Parse a single muuk.toml file representing a package
Result<void> MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
    muuk::logger::trace("Attempting to parse muuk.toml: {}", path);
    if (!fs::exists(path))
        return muuk::make_error<muuk::EC::FileNotFound>(path);

    auto result_muuk = muuk::parse_muuk_file(path);
    if (!result_muuk)
        return Err(result_muuk.error());

    auto data = result_muuk.value();

    const auto package_name = data["package"]["name"].as_string();
    const auto package_version = data["package"]["version"].as_string();
    const auto package_source = data.at("package").contains("git")
        ? data.at("package").at("git").as_string()
        : std::string();

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
        PackageType::LIBRARY);

    const auto base_path = fs::path(path).parent_path().string();

    // TODO: Check and move below into parse_dependencies
    auto deps_result = parse_dependencies(data, package);
    for (const auto& [dep_name, version_map] : package->dependencies_)
        for (const auto& [_, dep_ptr] : version_map)
            if (dep_ptr)
                package->all_dependencies_array.insert(dep_ptr);

    if (data.contains("library") && data["library"].is_table())
        package->library_config.load(
            package_name,
            package_version,
            base_path,
            data["library"].as_table());

    parse_features(data, package);

    package->source = package_source;

    if (data.contains("compiler"))
        package->compilers_config.load(data["compiler"], base_path);

    if (data.contains("platform"))
        package->compilers_config.load(data["platform"], base_path);

    resolved_packages[package_name][package_version] = package;

    if (is_base) {
        // TODO parse this out of regular libraries as well
        // and compare with the base package
        edition_ = muuk::Edition::from_string(data["package"]["edition"].as_string()).value_or(muuk::Edition::Unknown);
        parse_profile(data);

        base_package_ = package;

        for (const auto& [build_name, build_pkg] : data["build"].as_table()) {
            // TODO: Make sure build key exists
            auto build = std::make_shared<Build>();
            build->load(build_pkg, base_path);
            builds_[build_name] = build;
        }

        if (data.contains("profile"))
            for (const auto& [profile_name, profile_data] : data["profile"].as_table()) {
                package->profiles_config[profile_name] = {};
                package->profiles_config[profile_name].load(profile_data.as_table(), profile_name, base_path);
            }

    } // if (is_base)

    return {};
}

void MuukLockGenerator::generate_gitignore() {
    std::ofstream out("deps/.gitignore");
    if (!out.is_open()) {
        muuk::logger::warn("Failed to open deps/.gitignore for writing.");
        return;
    }

    out << "/*\n\n";

    // TODO: Probably more efficient way to do this
    for (const auto& [name, version] : resolved_order_) {
        // Skip base package
        if (base_package_ && name == base_package_->name && version == base_package_->version)
            continue;

        auto pkg = find_package(name, version);
        if (!pkg || pkg->package_type != PackageType::LIBRARY)
            continue;

        out << "!/" << name << "\n";
        out << "/" << name << "/*\n";
        out << "!/" << name << "/" << version << "\n";
        out << "/" << name << "/" << version << "/*\n";
        out << "!/" << name << "/" << version << "/muuk.toml\n\n";
    }

    out.close();
    muuk::logger::info(".gitignore generated at deps/.gitignore");
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

    if (!package) {
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

                if (version.has_value()) {
                    package = resolved_packages[package_name][version.value()];
                } else {
                    return Err("Version not specified for package '" + package_name + "'.");
                }

                if (!package) {
                    return Err("Package '{}' not found after parsing '{}'.", package_name, *search_path);
                }
            } else {
                return muuk::make_error<muuk::EC::FileNotFound>(search_file.string());
            }
        } else {
            // If no search path, search in dependency folders
            auto result = search_and_parse_dependency(package_name, version.value());
            if (!result)
                return Err(result);

            package = find_package(package_name, version.value());

            if (!package)
                return Err("Package '{}' not found after searching the dependency folder ({}).", package_name, DEPENDENCY_FOLDER);
        }
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
                auto result = resolve_dependencies(
                    dep_name,
                    dep_version,
                    dep_search_path.empty()
                        ? std::nullopt
                        : std::optional<std::string> { dep_search_path });

                if (!result)
                    return Err(result);
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

// TODO: Recursively merge packages
Result<void> MuukLockGenerator::merge_build_dependencies(
    const std::string& build_name,
    std::shared_ptr<Build> build,
    const Dependency& base_package_dep) {

    if (!build) {
        return Err("Build '{}' is null. Cannot merge dependencies.", build_name);
    }

    muuk::logger::info("Merging dependencies for build '{}'", build_name);

    // auto result_merge = merge_resolved_dependencies(build_name);
    // if (!result_merge) {
    //     return Err("Failed to merge resolved dependencies for build '{}': {}", build_name, result_merge.error());
    // }

    // Add base package to the build
    build->dependencies[base_package_->name][base_package_->version] = base_package_dep;
    build->all_dependencies_array.insert(std::make_shared<Dependency>(base_package_dep));
    build->merge(*base_package_);

    // Merge each dependency's resolved package into the build
    for (const auto& [dep_name, versions] : build->dependencies) {
        for (const auto& [dep_version, _] : versions) {
            if (resolved_packages.count(dep_name) && resolved_packages[dep_name].count(dep_version)) {
                const auto& dep_package = resolved_packages[dep_name][dep_version];
                build->merge(*dep_package);
            } else {
                muuk::logger::warn(
                    "Resolved package '{}' version '{}' not found when merging into build '{}'.",
                    dep_name,
                    dep_version,
                    build_name);
            }
        }
    }

    return {};
}

Result<void> MuukLockGenerator::merge_resolved_dependencies(const std::string& package_name, std::optional<std::string> version) {
    const auto package = find_package(package_name, version);
    if (!package) {
        muuk::logger::error("Package '{}' not found.", package_name);
        return {};
    }

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
Result<void> MuukLockGenerator::search_and_parse_dependency(const std::string& package_name, const std::string& version) {
    muuk::logger::info("Searching for target package '{}', version '{}'.", package_name, version);
    fs::path search_dir = fs::path(DEPENDENCY_FOLDER) / package_name / version;

    // TODO: Make this return matter
    if (!fs::exists(search_dir)) {
        return Err("Dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
    }

    fs::path dep_path = search_dir / "muuk.toml";
    if (fs::exists(dep_path)) {
        auto result = parse_muuk_toml(dep_path.string());
        if (!result) {
            return Err(result);
        }
    } else {
        return Err("muuk.toml for dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
    }
    return {};
}

Result<void> MuukLockGenerator::load() {
    // muuk::logger::info("------------------------------");
    muuk::logger::info("");
    muuk::logger::info(" Generating muuk.lock.toml...");
    muuk::logger::info("------------------------------");

    // TODO: Remove 2x parse
    auto result = parse_muuk_toml(base_path_ + "muuk.toml", true);
    if (!result) {
        return Err(result);
    }

    // TODO: Make it be base_path + "/" <= check for file
    // Extract the package name and version from the base muuk.toml
    auto base_muuk_result = muuk::parse_muuk_file(base_path_ + "muuk.toml");
    if (!base_muuk_result) {
        muuk::logger::error("Failed to parse muuk file for base muuk.toml");
        return Err("");
    }

    auto base_data = base_muuk_result.value();
    if (!base_data.contains("package") || !base_data["package"].is_table()) {
        muuk::logger::error("Missing 'package' section in base muuk.toml file.");
        return Err("");
    }

    const std::string base_package_name = base_data["package"]["name"].as_string();
    const std::string base_package_version = base_data["package"]["version"].as_string();
    muuk::logger::info("Base package name extracted: {}, version: {}", base_package_name, base_package_version);

    Dependency base_package_dep;
    base_package_dep.load(base_package_name, base_data);
    base_package_dep.version = base_package_version;

    // Resolve dependencies for the base package
    auto result_base_pkg = resolve_dependencies(
        base_package_name,
        base_package_version);
    if (!result_base_pkg)
        return Err(result_base_pkg);

    muuk::logger::info("Resolving dependencies for build packages...");
    for (const auto& [build_name, build] : builds_) {
        auto result_build_pkg = resolve_build_dependencies(build_name);
        if (!result_build_pkg)
            muuk::logger::error(
                "Failed to resolve dependencies for build package '{}': {}",
                build_name,
                result_build_pkg.error());
    }

    for (const auto& [package_name, version] : resolved_order_) {
        const auto package = find_package(package_name, version);
        if (!package)
            continue;

        for (const auto& [dep_name, version_map] : package->dependencies_)
            for (const auto& [dep_version, dep_info] : version_map)
                if (dependencies_.count(dep_name) && dependencies_[dep_name].count(dep_version)) {
                    auto dep = dependencies_[dep_name][dep_version];
                    auto package_dep = resolved_packages[dep_name][dep_version];
                    package_dep->enable_features(dep->enabled_features);
                }
    }

    merge_resolved_dependencies(base_package_name, base_package_version);

    for (const auto& [build_name, build] : builds_) {
        auto build_result = merge_build_dependencies(build_name, build, base_package_dep);
        if (!build_result) {
            muuk::logger::error("Failed to merge build dependencies for '{}': {}", build_name, build_result.error());
        }
    }

    propagate_profiles();

    return {};
}

Result<void> MuukLockGenerator::generate_cache(const std::string& output_path) {

    //   Write to cache file
    toml::value root = toml::table {};

    // Write Libraries
    toml::value library_array = toml::array {};
    for (const auto& [package_name, version] : resolved_order_) {
        const auto package = find_package(package_name, version);
        if (!package)
            continue;

        toml::value lib_table;
        package->library_config.serialize(lib_table);

        if (lib_table.contains("external"))
            lib_table.at("external").as_table_fmt().fmt = toml::table_format::oneline;

        if (lib_table.contains("sources"))
            lib_table.at("sources").as_array_fmt().fmt = toml::array_format::multiline;

        if (lib_table.contains("modules"))
            lib_table.at("modules").as_array_fmt().fmt = toml::array_format::multiline;

        library_array.as_array().push_back(lib_table);

        muuk::logger::info("Written package '{}' to lockfile.", package_name);
    }

    library_array.as_array_fmt().fmt = toml::array_format::array_of_tables;
    root["library"] = library_array;

    // Write builds
    toml::value build_array = toml::array {};
    for (const auto& [build_name, build_ptr] : builds_) {
        if (!build_ptr)
            continue;

        toml::value build_table;
        auto build_serial = build_ptr->serialize(build_table);
        if (!build_serial)
            return Err(build_serial);

        build_table["name"] = build_name;
        build_table["version"] = base_package_->version;

        if (build_table.contains("sources"))
            build_table.at("sources").as_array_fmt().fmt = toml::array_format::multiline;

        build_array.as_array().push_back(build_table);
    }

    build_array.as_array_fmt().fmt = toml::array_format::array_of_tables;

    root["build"] = build_array;

    // Write Profiles
    // if (base_package_ && !base_package_->profiles_config.empty()) {
    //     toml::value profile_section = toml::table {};

    //     for (const auto& [profile_name, profile_cfg] : base_package_->profiles_config) {
    //         toml::value profile_data = toml::table {};
    //         profile_cfg.serialize(profile_data);
    //         profile_section[profile_name] = profile_data;
    //     }

    //     root["profile"] = profile_section;
    // }

    // Write To Cachefile
    std::ofstream lockfile(output_path);
    if (!lockfile) {
        muuk::logger::error("Failed to open lockfile: {}", output_path);
        return Err("");
    }

    lockfile << toml::format(root);

    // TODO: Use PROFILES from base_config.hpp
    if (!profiles_.empty()) {
        for (const auto& [profile_name, profile_settings] : profiles_) {
            lockfile << "[profile." << profile_name << "]\n";

            for (const auto& [setting_name, values] : profile_settings) {
                toml::array value_array;
                for (const auto& value : values) {
                    value_array.push_back(value);
                }
                lockfile << setting_name << " = " << toml::format(toml::value(value_array)) << "\n";
            }

            lockfile << "\n";
        }
    }

    lockfile.close();
    return {};
}

// Generates the muuk.lock.toml file by parsing the base muuk.toml, resolving dependencies,
// and writing the resolved packages and profiles to the lockfile.
Result<void> MuukLockGenerator::generate_lockfile(const std::string& output_path) {

    //   Write to cargo-style muuk.lock
    std::ofstream cargo_style_lock(output_path);
    if (!cargo_style_lock) {
        muuk::logger::error("Failed to open muuk.lock for writing.");
        return Err("");
    }

    cargo_style_lock << "# This file is automatically @generated by Muuk.\n\n";

    std::set<std::pair<std::string, std::string>> written_packages;

    for (const auto& [build_name, build_ptr] : builds_) {
        if (!build_ptr) {
            muuk::logger::warn("Build pointer for '{}' is null.", build_name);
            continue;
        }

        for (const auto& dep_ptr : build_ptr->all_dependencies_array) {
            if (!dep_ptr) {
                muuk::logger::warn("Dependency pointer for '{}' is null.", build_name);
                continue;
            }

            const auto& dep_name = dep_ptr->name;
            const auto& dep_version = dep_ptr->version;

            // Skip base package
            if (base_package_ && dep_name == base_package_->name && dep_version == base_package_->version)
                continue;

            if (written_packages.count({ dep_name, dep_version }))
                continue;

            auto package = find_package(dep_name, dep_version);
            if (!package)
                continue;

            written_packages.insert({ dep_name, dep_version });

            const auto& source = !package->source.empty()
                ? package->source
                : dep_ptr->git_url;

            cargo_style_lock << "[[package]]\n";
            cargo_style_lock << "name = \"" << dep_name << "\"\n";
            cargo_style_lock << "version = \"" << dep_version << "\"\n";

            // TODO: better way of differentiating between source and path
            // if (!dep_ptr->git_url.empty()) {
            cargo_style_lock << "source = \"git+" << source << "\"\n";
            // }
            if (!dep_ptr->path.empty()) {
                cargo_style_lock << "source = \"path+" << dep_ptr->path << "\"\n";
            }

            if (!dep_ptr->enabled_features.empty()) {
                cargo_style_lock << "features = [";
                bool first = true;
                for (const auto& feature : dep_ptr->enabled_features) {
                    if (!first)
                        cargo_style_lock << ", ";
                    cargo_style_lock << "\"" << feature << "\"";
                    first = false;
                }
                cargo_style_lock << "]\n";
            }

            if (!package->dependencies_.empty()) {
                cargo_style_lock << "dependencies = [\n";
                for (const auto& [child_name, ver_map] : package->dependencies_) {
                    for (const auto& [child_ver, _] : ver_map) {
                        cargo_style_lock << "  { name = \"" << child_name << "\", version = \"" << child_ver << "\" },\n";
                    }
                }
                cargo_style_lock << "]\n";
            }

            cargo_style_lock << "\n";
        }
    }

    cargo_style_lock.close();

    generate_gitignore();

    muuk::logger::info("muuk.lock.toml generation complete!");
    return {};
}

// TODO: Replace std::shared_ptr<Package> with unique_ptr and Package*
// Finds and returns a package by its name from the resolved packages.
std::shared_ptr<Package> MuukLockGenerator::find_package(const std::string& package_name, std::optional<std::string> version) {
    if (resolved_packages.count(package_name) && version.has_value()) {
        return resolved_packages[package_name][version.value()];
    }

    if (builds.count(package_name)) {
        return builds[package_name];
    }
    return nullptr;
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
    //     if (!include_path.empty() && util::path_exists(include_path)) {
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

void MuukLockGenerator::parse_and_store_flag_categories(
    const toml::value& data,
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>& target,
    const std::vector<std::string>& flag_categories) {
    for (const auto& [category_name, category_data] : data.as_table()) {
        if (!category_data.is_table()) {
            continue;
        }

        const std::string category_name_str = category_name;

        for (const auto& flag_type : flag_categories) {
            target[category_name_str][flag_type] = muuk::parse_array_as_set(
                category_data, flag_type);
        }
    }
}

Result<void> MuukLockGenerator::resolve_build_dependencies(const std::string& build_name) {
    if (visited_builds.count(build_name)) {
        muuk::logger::trace("Build '{}' already processed. Skipping resolution.", build_name);
        return {};
    }

    visited_builds.insert(build_name);
    muuk::logger::info("Resolving dependencies for build target '{}'", build_name);

    if (!builds_.count(build_name))
        return Err("Build target '{}' not found in build map.", build_name);

    auto build_config = builds_[build_name];
    if (!build_config) {
        return Err("Build target '{}' is null in build map.", build_name);
    }

    for (const auto& dep_ptr : build_config->all_dependencies_array) {
        if (!dep_ptr) {
            muuk::logger::warn("Null dependency pointer in build '{}'", build_name);
            continue;
        }

        const auto& dep_name = dep_ptr->name;
        const auto& dep_version = dep_ptr->version;

        std::string dep_search_path;
        if (!dep_ptr->path.empty()) {
            dep_search_path = dep_ptr->path;
            muuk::logger::info("Using specified path for build dependency '{}': {}", dep_name, dep_search_path);
        }

        if (dep_ptr->system) {
            // TODO
            // resolve_system_dependency(dep_name, build_config);
        } else {
            auto result = resolve_dependencies(
                dep_name,
                dep_version,
                dep_search_path.empty()
                    ? std::nullopt
                    : std::optional<std::string> { dep_search_path });

            if (!result)
                return Err(
                    "Failed to resolve dependency '{}' for build '{}': {}",
                    dep_name,
                    build_name,
                    result.error());
        }

        if (resolved_packages.count(dep_name) && resolved_packages[dep_name].count(dep_version)) {
            const auto dep_pkg = resolved_packages[dep_name][dep_version];
            build_config->merge(*dep_pkg);
            build_config->all_dependencies_array.insert(dep_ptr);
            muuk::logger::info(
                "Merged dependency '{}' (v{}) into build '{}'",
                dep_name,
                dep_version,
                build_name);
        } else
            muuk::logger::warn(
                "Dependency '{}' (v{}) not found in resolved_packages after resolution.",
                dep_name,
                dep_version);
    }

    resolved_order_.emplace_back(build_name, base_package_->version);
    return {};
}

void MuukLockGenerator::propagate_profiles() {
    muuk::logger::info("Propagating profiles from builds to dependent libraries...");

    // Start from builds
    for (const auto& [build_name, build] : builds_) {
        if (!build)
            continue;

        std::unordered_set<std::string> build_profiles = build->profiles;

        for (const auto& dep_ptr : build->all_dependencies_array) {
            if (!dep_ptr)
                continue;

            const auto& dep_name = dep_ptr->name;
            const auto& dep_version = dep_ptr->version;

            auto dep_package = find_package(dep_name, dep_version);
            if (!dep_package)
                continue;

            if (dep_package->package_type == PackageType::LIBRARY) {
                propagate_profiles_downward(*dep_package, build_profiles);
            }
        }
    }
}

void MuukLockGenerator::propagate_profiles_downward(Package& package, const std::unordered_set<std::string>& inherited_profiles) {
    if (package.package_type != PackageType::LIBRARY)
        return;

    // Insert inherited profiles
    package.library_config.profiles.insert(inherited_profiles.begin(), inherited_profiles.end());

    // Recursively apply to its dependencies
    for (const auto& [dep_name, version_map] : package.dependencies_) {
        for (const auto& [dep_version, dep_info] : version_map) {
            if (!resolved_packages.count(dep_name) || !resolved_packages[dep_name].count(dep_version))
                continue;

            auto& dep_pkg = resolved_packages[dep_name][dep_version];
            if (dep_pkg->package_type == PackageType::LIBRARY) {
                propagate_profiles_downward(*dep_pkg, inherited_profiles);
            }
        }
    }
}