#include "muuklockgen.h"
#include "logger.h"
#include "buildconfig.h"
#include "util.h"
#include "muuk.h"

#include <glob/glob.h>
#include <tl/expected.hpp>
#include <toml++/toml.h>
#include <fmt/core.h>

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) : base_path_(base_path) {
    muuk::logger::trace("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages = {};

    module_parser_ = std::make_unique<MuukModuleParser>(base_path);
}

// Parse a single muuk2.toml file representing a package
void MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
    muuk::logger::trace("Attempting to parse muuk2.toml: {}", path);
    if (!fs::exists(path)) {
        muuk::logger::error("Error: '{}' not found!", path);
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

    std::string package_name = data["package"]["name"].value_or<std::string>("unknown");
    std::string package_version = data["package"]["version"].value_or<std::string>("0.0.0");

    muuk::logger::info("Parsing muuk2.toml for package: {} ({})", package_name, package_version);

    muuk::logger::info("Parsing package: {} (version: {}) with muuk path: `{}`", package_name, package_version, path);

    auto package = std::make_shared<Component>(std::string(package_name), std::string(package_version),
        fs::path(path).parent_path().string(), "library");

    parse_dependencies(data, package);
    parse_library(data, package);
    parse_platform(data, package);
    parse_compiler(data, package);

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

    std::optional<std::shared_ptr<Component>> package_opt = find_package(package_name, version);

    if (!package_opt.has_value()) {
        if (search_path) {
            fs::path search_file = fs::path(search_path.value());
            if (!search_path.value().ends_with("muuk2.toml")) {
                muuk::logger::info("Search path '{}' does not end with 'muuk2.toml', appending it.", search_file.string());
                search_file /= "muuk2.toml";
            }
            else {
                muuk::logger::info("Search path '{}' already ends with 'muuk2.toml', using as is.", search_file.string());
            }

            if (fs::exists(search_file)) {
                parse_muuk_toml(search_file.string());
                package_opt = resolved_packages[package_name][version.value_or("")];

                if (version.has_value()) {
                    package_opt = resolved_packages[package_name][version.value()];
                }
                else {
                    muuk::logger::error("Version not specified for package '" + package_name + "'.");
                    return Err("Version not specified for package '" + package_name + "'.");
                }

                if (!package_opt) {
                    muuk::logger::error("Error: Component '{}' not found after parsing '{}'.", package_name, *search_path);
                    return Err("");
                }
            }
            else {
                muuk::logger::error("Error: Search file '{}' for '{}' does not exist.", search_file.string(), package_name);
                return Err("");
            }
        }
        else {
            // If no search path, search in dependency folders
            search_and_parse_dependency(package_name, version.value());
            package_opt = resolved_packages[package_name][version.value()];

            if (!package_opt.has_value()) {
                muuk::logger::error("Error: Component '{}' not found after searching the dependency folder ({}).", package_name, DEPENDENCY_FOLDER);
                return Err("");
            }
        }
        // muuk::logger::info("Component '{}' not found in resolved packages.", package_name);
    }

    auto package = package_opt.value();  // Safe extraction since we checked before

    for (const auto& [dep_name, version_map] : package->dependencies_) {
        for (const auto& [dep_version, dep_info] : version_map) {

            if (dep_name == package_name) {
                muuk::logger::warn("Circular dependency detected: '{}' depends on itself. Skipping.", package_name);
                continue;
            }

            muuk::logger::info("Resolving dependency '{}' for '{}'", dep_name, package_name);

            std::string dep_search_path;
            if (dep_info.count("muuk_path")) {
                dep_search_path = dep_info.at("muuk_path");
                muuk::logger::info("Using defined muuk_path for dependency '{}': {}", dep_name, dep_search_path);
            }

            if (dep_info.count("system")) {
                resolve_system_dependency(dep_name, package);
            }
            else {
                // Resolve dependency, passing the search path if available
                auto result = resolve_dependencies(
                    dep_name,
                    dep_version,
                    dep_search_path.empty() ? std::nullopt : std::optional<std::string>{ dep_search_path }
                );

                if (!result) {
                    std::string error_msg = "Failed to resolve dependency '" + dep_name + "' for package '" + package_name + "'.";
                    muuk::logger::error(error_msg);
                    return Err("");
                }
            }

            if (
                resolved_packages.count(dep_name) &&
                resolved_packages[dep_name].count(dep_version)
                ) {
                muuk::logger::info("Merging '{}' into '{}'", dep_name, package_name);
                if (package) {
                    package->merge(*resolved_packages[dep_name][dep_version]);
                }
            }

            toml::table dependency_entry;
            for (const auto& [key, value] : dep_info) {
                dependency_entry.insert(key, value);
            }

            dependencies_[dep_name][dep_version] = dependency_entry;
            // dependencies_[dep_name][version] = dependency_entry;

            // Log full dependency details
            muuk::logger::info("Resolved dependency '{}':", dep_name);
            for (const auto& [key, val] : dep_info) {
                muuk::logger::info("  - {}: {}", key, val);
            }
        }
    }

    muuk::logger::info("Added '{}' to resolved order list.", package_name);
    resolved_order_.push_back(std::make_pair(package_name, version.value_or("unknown")));
    return {};
}

// Searches for the specified package in the dependency folder and parses its muuk2.toml file if found.
void MuukLockGenerator::search_and_parse_dependency(const std::string& package_name, const std::string& version) {
    muuk::logger::info("Searching for target package '{}', version '{}'.", package_name, version);
    fs::path search_dir = fs::path(DEPENDENCY_FOLDER) / package_name / version;

    if (!fs::exists(search_dir)) {
        muuk::logger::warn("Dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
        return;
    }

    fs::path dep_path = search_dir / "muuk2.toml";
    if (fs::exists(dep_path)) {
        parse_muuk_toml(dep_path.string());
    }
    else {
        muuk::logger::warn("muuk2.toml for dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());
    }
}

// Generates the muuk.lock.toml file by parsing the base muuk2.toml, resolving dependencies,
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

    parse_muuk_toml(base_path_ + "muuk2.toml", true);

    // TODO Take directly from `parse_muuk_toml`
    // Extract the package name and version from the base muuk2.toml
    auto base_muuk_filer = MuukFiler::create(base_path_ + "muuk2.toml");
    if (!base_muuk_filer) {
        muuk::logger::error("Failed to create MuukFiler for base muuk2.toml");
        return;
    }

    auto base_data = base_muuk_filer->get_config();
    if (!base_data.contains("package") || !base_data["package"].is_table()) {
        muuk::logger::error("Missing 'package' section in base muuk2.toml file.");
        return;
    }

    // TODO: Return Err if unknown
    std::string base_package_name = base_data["package"]["name"].value_or<std::string>("unknown");
    std::string base_package_version = base_data["package"]["version"].value_or<std::string>("0.0.0");
    muuk::logger::info("Base package name extracted: {}, version: {}", base_package_name, base_package_version);

    // Resolve dependencies for the base package
    resolve_dependencies(base_package_name, base_package_version);

    muuk::logger::info("Resolving dependencies for build packages...");
    for (const auto& [build_name, build] : builds) {
        auto result = resolve_dependencies(build_name);
        if (!result) {
            muuk::logger::error(
                "Failed to resolve dependencies for build package '{}': {}",
                build_name,
                result.error()
            );
        }

        build->deps.insert(base_package_name);
        build->merge(*base_package_);
    }


    for (const auto& [package_name, version] : resolved_order_) {
        auto package_opt = find_package(package_name, version);
        if (!package_opt) continue;

        auto package = package_opt.value();
        std::string package_type = package->package_type;

        std::string package_table = package->serialize();

        lockfile << "[[" << package_type << "]]\nname = \"" << package_name << "\"\n";
        lockfile << package_table << "\n";

        muuk::logger::info("Written package '{}' to lockfile.", package_name);
    }

    lockfile << MuukFiler::format_dependencies(dependencies_);

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
std::optional<std::shared_ptr<Component>> MuukLockGenerator::find_package(const std::string& package_name, std::optional<std::string> version) {
    if (resolved_packages.count(package_name) && version.has_value()) {
        return resolved_packages[package_name][version.value()];
    }

    if (builds.count(package_name)) {
        return builds[package_name];
    }
    return std::nullopt;
}

// Processes the given module paths and adds the resolved modules to the package.
void MuukLockGenerator::process_modules(const std::vector<std::string>& module_paths, Component& package) {
    muuk::logger::info("Processing modules for package: {}", package.name);

    module_parser_->parseAllModules(module_paths);

    std::vector<std::string> resolved_modules = module_parser_->resolveAllModules();

    for (const auto& mod : resolved_modules) {
        package.modules.push_back(mod);
        muuk::logger::info("Resolved and added module '{}' to package '{}'", mod, package.name);
    }
}

// Resolves system dependencies by checking for include and library paths for the given package name.
void MuukLockGenerator::resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Component>> package) {
    muuk::logger::info("Checking system dependency: '{}'", package_name);

    std::string include_path, lib_path;

    // Special handling for Python
    if (package_name == "python" || package_name == "python3") {
#ifdef _WIN32
        include_path = util::execute_command_get_out("python -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::execute_command_get_out("python -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#else
        include_path = util::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#endif
    }
    // Special handling for Boost
    else if (package_name == "boost") {
#ifdef _WIN32
        include_path = util::execute_command_get_out("where boostdep");
        lib_path = util::execute_command_get_out("where boost");
#else
        include_path = util::execute_command("boostdep --include-path");
        lib_path = util::execute_command("boostdep --lib-path");
#endif
    }
    else {
#ifdef _WIN32
        muuk::logger::warn("System dependency '{}' resolution on Windows is not fully automated. Consider setting paths manually.", package_name);
#else
        include_path = util::execute_command("pkg-config --cflags-only-I " + package_name + " | sed 's/-I//' | tr -d '\n'");
        lib_path = util::execute_command("pkg-config --libs-only-L " + package_name + " | sed 's/-L//' | tr -d '\n'");
#endif
}

    if (!include_path.empty() && util::path_exists(include_path)) {
        system_include_paths_.insert(include_path);
        if (package) (*package)->add_include_path(include_path);
        muuk::logger::info("  - Found Include Path: {}", include_path);
    }
    else {
        muuk::logger::warn("  - Include path for '{}' not found.", package_name);
    }

    if (!lib_path.empty() && fs::exists(lib_path)) {
        system_library_paths_.insert(lib_path);
        if (package) (*package)->libs.push_back(lib_path);
        muuk::logger::info("  - Found Library Path: {}", lib_path);
}
    else {
        muuk::logger::warn("  - Library path for '{}' not found.", package_name);
    }

    if (include_path.empty() && lib_path.empty()) {
        muuk::logger::error("Failed to resolve system dependency '{}'. Consider installing it or specifying paths manually.", package_name);
    }
}

// Merges the settings of the inherited profile into the base profile.
void MuukLockGenerator::merge_profiles(const std::string& base_profile, const std::string& inherited_profile) {
    if (profiles_.find(inherited_profile) == profiles_.end()) {
        muuk::logger::error("Inherited profile '{}' not found.", inherited_profile);
        return;
    }

    muuk::logger::trace("Merging profile '{}' into '{}'", inherited_profile, base_profile);

    for (const auto& [key, values] : profiles_[inherited_profile]) {
        profiles_[base_profile][key].insert(values.begin(), values.end());
    }
}
