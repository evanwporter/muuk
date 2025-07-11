#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <toml.hpp>

#include "buildconfig.h"
#include "compiler.hpp"
#include "lockgen/muuklockgen.hpp"
#include "logger.hpp"
#include "muuk.hpp"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "toml_ext.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {
    namespace lockgen {

        MuukLockGenerator::MuukLockGenerator(const std::string& base_path) :
            base_path_(base_path) {
            muuk::logger::trace("MuukLockGenerator initialized with base path: {}", base_path_);

            resolved_packages = {};
        }

        Result<MuukLockGenerator> MuukLockGenerator::create(const std::string& base_path) {
            auto lockgen = MuukLockGenerator(base_path);
            TRYV(lockgen.load());
            return lockgen;
        }

        /// Generates a Package object from the parsed muuk.toml file.
        Result<void> MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
            muuk::logger::trace("Attempting to parse muuk.toml: {}", path);

            auto result_muuk = muuk::parse_muuk_file(path);
            if (!result_muuk)
                return Err(result_muuk);

            auto data = result_muuk.value();

            const auto package_name = data["package"]["name"].as_string();

            return parse_muuk_toml(data, path, is_base);
        }

        /// Generates a Package object from the parsed muuk.toml value.
        Result<void> MuukLockGenerator::parse_muuk_toml(const toml::basic_value<toml::ordered_type_config>& data, const std::string& path, bool is_base) {

            const auto package_name = data.at("package").at("name").as_string();
            const auto package_version = data.at("package").at("version").as_string();
            const auto package_source = data.at("package").contains("git")
                ? data.at("package").at("git").as_string()
                : std::string();

            muuk::logger::info(
                "Parsing package: {} (version: {}) with muuk path: `{}`",
                package_name,
                package_version,
                path);

            auto package = std::make_shared<Package>(
                package_name,
                package_version,
                fs::path(path).parent_path().string());

            const auto base_path = fs::path(path).parent_path().string();

            auto deps_result = parse_dependencies(data, package);
            for (const auto& [dep_name, version_map] : package->dependencies_)
                for (const auto& [_, dep_ptr] : version_map)
                    if (dep_ptr)
                        package->all_dependencies_array.insert(dep_ptr);

            if (data.contains("library") && data.at("library").is_table())
                package->library_config.load(
                    package_name,
                    package_version,
                    base_path,
                    data.at("library").as_table());

            // TODO: Perhaps make it mutually exclusive with `library`?
            // TODO: Or combine them?
            if (data.contains("external") && data.at("external").is_table())
                package->external_config.load(
                    package_name,
                    package_version,
                    base_path,
                    data.at("external").as_table());

            parse_features(data, package);

            package->source = package_source;

            if (data.contains("compiler"))
                package->compilers_config.load(data.at("compiler"), base_path);

            if (data.contains("platform"))
                package->platforms_config.load(data.at("platform"), base_path);

            auto edition = CXX_Standard::from_string(
                toml::try_find_or(
                    data.at("package"),
                    "cxx_standard",
                    std::string {}));

            // If this is the base project, just store its edition
            if (is_base) {
                base_cxx_standard = edition;
            }

            // If one of the dependencies requires a newer standard, upgrade the base standard
            else {
                if (edition > base_cxx_standard) {
                    muuk::logger::warn(
                        "Dependency '{}' (v{}) requires C++ standard {}, which is newer than base project's standard {}. "
                        "Upgrading base standard to {}.",
                        package_name,
                        package_version,
                        edition.to_string(),
                        base_cxx_standard.to_string(),
                        edition.to_string());
                    base_cxx_standard = edition;
                }
            }

            resolved_packages[package_name][package_version] = package;

            if (is_base) {
                parse_profile(data);

                base_package_ = package;

                for (const auto& [build_name, build_pkg] : data.at("build").as_table()) {
                    // TODO: Make sure build key exists
                    auto build = std::make_shared<Build>();
                    build->load(build_pkg, base_path);
                    builds_[build_name] = build;
                }

            } // if (is_base)

            return {};
        }

        void MuukLockGenerator::generate_gitignore() {
            std::ofstream out(DEPENDENCY_FOLDER + "/.gitignore");
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
                if (!pkg)
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

        // TODO: Recursively merge packages
        Result<void> MuukLockGenerator::merge_build_dependencies(
            const std::string& build_name,
            std::shared_ptr<Build> build,
            const Dependency& base_package_dep) {

            if (!build) // This should never happen, but just in case
                return Err("Build '{}' is null. Cannot merge dependencies.", build_name);

            muuk::logger::info("Merging dependencies for build '{}'", build_name);

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

        Result<void> MuukLockGenerator::search_and_parse_dependency(const std::string& package_name, const std::string& version) {
            muuk::logger::info("Searching for target package '{}', version '{}'.", package_name, version);
            fs::path search_dir = fs::path(DEPENDENCY_FOLDER) / package_name / version;

            // TODO: Make this return matter
            if (!fs::exists(search_dir))
                return Err("Dependency '{}' version '{}' not found in '{}'", package_name, version, search_dir.string());

            fs::path dep_path = search_dir / MUUK_TOML_FILE;

            if (!fs::exists(dep_path))
                return Err(
                    "{} for dependency '{}' version '{}' not found in '{}'",
                    MUUK_TOML_FILE,
                    package_name,
                    version,
                    search_dir.string());

            auto result_muuk = muuk::parse_muuk_file(dep_path.string());
            if (!result_muuk)
                return Err(result_muuk);

            const auto& data = result_muuk.value();

            const auto actual_name = data.at("package").at("name").as_string();
            const auto actual_version = data.at("package").at("version").as_string();

            if (actual_name != package_name || actual_version != version) {
                return Err(
                    // TODO: Better error message
                    "Mismatch in dependency at '{}': expected '{}@{}', found '{}@{}' in `{}`.",
                    dep_path.string(),
                    package_name,
                    version,
                    actual_name,
                    actual_version,
                    MUUK_TOML_FILE);
            }

            TRYV(parse_muuk_toml(data, dep_path.string()));

            return {};
        }

        Result<void> MuukLockGenerator::load() {
            // muuk::logger::info("------------------------------");
            muuk::logger::info("");
            muuk::logger::info(" Generating muuk.lock.toml...");
            muuk::logger::info("------------------------------");

            // TODO: Make it be base_path + "/" <= check for file
            // Extract the package name and version from the base muuk.toml
            auto base_muuk_result = muuk::parse_muuk_file(base_path_ + MUUK_TOML_FILE);
            if (!base_muuk_result)
                Err(base_muuk_result);

            const auto base_data = base_muuk_result.value();

            TRYV(parse_muuk_toml(base_data, base_path_ + MUUK_TOML_FILE, true));

            const std::string base_package_name = base_data.at("package").at("name").as_string();
            const std::string base_package_version = base_data.at("package").at("version").as_string();
            muuk::logger::info(
                "Base package name extracted: {}, version: {}",
                base_package_name,
                base_package_version);

            Dependency base_package_dep;
            base_package_dep.load(base_package_name, base_data);
            base_package_dep.version = base_package_version;

            // Resolve dependencies for the base package
            TRYV(resolve_dependencies(
                base_package_name,
                base_package_version));

            muuk::logger::info("Resolving dependencies for build packages...");
            for (const auto& [build_name, build] : builds_) {
                TRYV(resolve_build_dependencies(build_name));
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

                // Apply default features
                muuk::logger::info("Applying default features for all resolved packages...");
                for (const auto& [pkg_name, versions] : resolved_packages)
                    for (const auto& [pkg_version, pkg_ptr] : versions) {
                        if (!pkg_ptr)
                            continue;

                        if (!pkg_ptr->default_features.empty()) {
                            muuk::logger::info("  -> Applied default features for package '{}': {}", pkg_name, fmt::join(pkg_ptr->default_features, ", "));
                            pkg_ptr->enable_features(pkg_ptr->default_features);
                        }
                    }
            }

            TRYV(merge_resolved_dependencies(
                base_package_name,
                base_package_version));

            for (const auto& [build_name, build] : builds_)
                TRYV(merge_build_dependencies(build_name, build, base_package_dep));

            propagate_profiles();

            return {};
        }

        Result<void> MuukLockGenerator::generate_cache(const std::string& output_path) {

            // Write to cache file
            toml::value root = toml::table {};

            // Write Libraries
            toml::value library_array = toml::array {};
            for (const auto& [package_name, version] : resolved_order_) {
                const auto package = find_package(package_name, version);
                if (!package)
                    continue;

                toml::value lib_table;
                package->library_config.serialize(
                    lib_table,
                    package->platforms_config,
                    package->compilers_config);

                lib_table["path"] = util::file_system::to_unix_path(package->base_path);

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

            // Write external libraries
            toml::value external_array = toml::array {};
            for (const auto& [package_name, version] : resolved_order_) {
                const auto package = find_package(package_name, version);
                if (!package)
                    continue;

                toml::value external_table = toml::table {};
                package->external_config.serialize(
                    external_table);

                // TODO: flattern it a bunch
                if (external_table.is_table()) {
                    auto& table = external_table.as_table();
                    if (table.contains("outputs") && table.at("outputs").is_array()) {
                        table.at("outputs").as_array_fmt().fmt = toml::array_format::multiline;
                    }
                }

                // TODO: More robust way of checking for external
                if (external_table.contains("name"))
                    external_array.as_array().push_back(external_table);

                muuk::logger::info("Written external package '{}' to lockfile.", package_name);
            }

            external_array.as_array_fmt().fmt = toml::array_format::array_of_tables;
            if (!external_array.is_empty())
                root["external"] = external_array;

            // Write builds
            toml::value build_array = toml::array {};
            for (const auto& [build_name, build_ptr] : builds_) {
                if (!build_ptr)
                    continue;

                toml::value build_table;
                TRYV(build_ptr->serialize(build_table));

                build_table["name"] = build_name;
                build_table["version"] = base_package_->version;

                if (build_table.contains("sources"))
                    build_table.at("sources").as_array_fmt().fmt = toml::array_format::multiline;

                build_array.as_array().push_back(build_table);
            }

            build_array.as_array_fmt().fmt = toml::array_format::array_of_tables;

            root["build"] = build_array;

            // Write Profiles
            if (!profiles_config_.empty()) {
                toml::value profile_section = toml::table {};

                for (const auto& [profile_name, profile_cfg] : profiles_config_) {
                    toml::value profile_data = toml::table {};

                    // Serialize the profile fields (sources, defines, etc.)
                    profile_cfg.serialize(profile_data);

                    profile_section[profile_name] = profile_data;
                }

                root["profile"] = profile_section;
            }

            // Write To Cachefile
            std::ofstream lockfile(output_path);
            if (!lockfile) {
                muuk::logger::error("Failed to open lockfile: {}", output_path);
                return Err("");
            }

            lockfile << toml::format(root);

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

                    cargo_style_lock << "[[package]]\n";
                    cargo_style_lock << "name = \"" << dep_name << "\"\n";
                    cargo_style_lock << "version = \"" << dep_version << "\"\n";

                    // TODO: better way of differentiating between source and path
                    if (!dep_ptr->path.empty()) {
                        cargo_style_lock << "source = \"path+" << dep_ptr->path << "\"\n";
                    } else if (!dep_ptr->git_url.empty()) {
                        cargo_style_lock << "source = \"git+" << dep_ptr->git_url << "\"\n";
                    } else if (!package->source.empty()) {
                        if (util::git::is_git_url(package->source))
                            cargo_style_lock << "source = \"git+" << package->source << "\"\n";
                        else // assume it's a path
                            cargo_style_lock << "source = \"path+" << package->source << "\"";
                    } else {
                        muuk::logger::warn("No source or path found for package `{}`.", dep_name);
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

            return nullptr;
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
                            result.error()); // Custom error handling so I can add the build name.
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
                        "Dependency '{}' (v{}) not found in resolved_packages after resolution for Build {}.",
                        dep_name,
                        dep_version,
                        build_name);
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

                    propagate_profiles_downward(*dep_package, build_profiles);
                }
            }
        }

        void MuukLockGenerator::propagate_profiles_downward(Package& package, const std::unordered_set<std::string>& inherited_profiles) {
            muuk::logger::info("Propagating profiles to package '{}'", package.name);

            // Insert inherited profiles
            package.library_config.profiles.insert(inherited_profiles.begin(), inherited_profiles.end());

            // Recursively apply to its dependencies
            for (const auto& [dep_name, version_map] : package.dependencies_) {
                for (const auto& [dep_version, dep_info] : version_map) {
                    if (!resolved_packages.count(dep_name) || !resolved_packages[dep_name].count(dep_version))
                        continue;

                    auto& dep_pkg = resolved_packages[dep_name][dep_version];
                    propagate_profiles_downward(*dep_pkg, inherited_profiles);
                }
            }
        }
    } // namespace lockgen
} // namespace muuk