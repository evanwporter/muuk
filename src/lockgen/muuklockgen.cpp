#include "muuklockgen.h"
#include "logger.h"
#include "buildconfig.h"
#include "util.h"
#include "tl/expected.hpp"

#include <glob/glob.h>

// Joins a vector of strings into a single string with a specified delimiter.
static inline std::string join_strings(const std::vector<std::string>& elements, const std::string& delimiter) {
    if (elements.empty()) return "";

    std::ostringstream oss;
    for (size_t i = 0; i < elements.size(); ++i) {
        oss << elements[i];
        if (i != elements.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) : base_path_(base_path) {
    muuk::logger::trace("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages_["library"] = {};
    resolved_packages_["build"] = {};

    module_parser_ = std::make_unique<MuukModuleParser>(base_path);
}

tl::expected<void, std::string>
MuukLockGenerator::parse_muuk_toml(
    const std::string& path,
    bool is_base
) {
    muuk::logger::trace("Attempting to parse muuk.toml: {}", path);
    if (!fs::exists(path)) {
        return tl::unexpected("Error: '" + path + "' not found!");
    }

    MuukFiler muuk_filer(path);
    auto data = muuk_filer.get_config();

    if (!data.contains("package") || !data["package"].is_table()) {
        return tl::unexpected("Missing 'package' section in TOML file.");
    }

    std::string package_name = data["package"]["name"].value_or<std::string>("unknown");
    std::string package_version = data["package"]["version"].value_or<std::string>("0.0.0");

    muuk::logger::info("Parsing muuk.toml for package: {} ({})", package_name, package_version);

    muuk::logger::info("Parsing package: {} (version: {}) with muuk path: `{}`", package_name, package_version, path);

    auto package = std::make_shared<Package>(std::string(package_name), std::string(package_version),
        fs::path(path).parent_path().string(), "library");

    if (data.contains("library")) {
        const auto& library_node = data["library"];
        if (const toml::table* library_table = library_node.as_table()) {
            if (library_table->contains(package_name)) { // TODO: Allow multiple libraries in a file
                parse_section(*library_table->at(package_name).as_table(), *package);

                if (data.contains("platform") && data["platform"].is_table()) {
                    for (const auto& [platform_name, platform_data] : *data["platform"].as_table()) {
                        if (platform_data.is_table() && platform_data.as_table()->contains("cflags")) {
                            const auto& cflag_array = *platform_data.as_table()->at("cflags").as_array();
                            std::set<std::string> cflags;
                            for (const auto& cflag : cflag_array) {
                                cflags.insert(*cflag.value<std::string>());
                            }

                            package->platform_[std::string(platform_name.str())]["cflags"] = cflags;

                            muuk::logger::info("Platform '{}' loaded with {} cflags.", platform_name.str(), cflags.size());
                        }
                    }
                }

                if (data.contains("compiler") && data["compiler"].is_table()) {
                    for (const auto& [compiler_name, compiler_data] : *data["compiler"].as_table()) {
                        if (compiler_data.is_table() && compiler_data.as_table()->contains("cflags")) {
                            const auto& cflag_array = *compiler_data.as_table()->at("cflags").as_array();
                            std::set<std::string> cflags;
                            for (const auto& cflag : cflag_array) {
                                cflags.insert(*cflag.value<std::string>());
                            }

                            package->compiler_[std::string(compiler_name.str())]["cflags"] = cflags;

                            muuk::logger::info("Compiler '{}' loaded with {} cflags.", compiler_name.str(), cflags.size());
                        }
                    }
                }
            }
        }
        else {
            muuk::logger::warn("No 'library' section found in TOML for {}", package_name);
        }
    }

    resolved_packages_["library"][package_name] = package;

    if (is_base) {
        if (data.contains("build") && data["build"].is_table()) {
            for (const auto& [build_name, build_info] : *data["build"].as_table()) {
                muuk::logger::info("Found build target: {}", build_name.str());

                auto build_package = std::make_shared<Package>(
                    std::string(build_name.str()),
                    std::string(package_version),
                    fs::path(path).parent_path().string(),
                    "build"
                );

                parse_section(*build_info.as_table(), *build_package);

                if (build_info.is_table() && build_info.as_table()->contains("profile")) {
                    const auto& profile_array = *build_info.as_table()->at("profile").as_array();
                    for (const auto& profile : profile_array) {
                        build_package->inherited_profiles.insert(*profile.value<std::string>());
                        muuk::logger::info(std::format("  → Build '{}' inherits profile '{}'", build_name.str(), *profile.value<std::string>()));
                    }
                }

                resolved_packages_["build"][std::string(build_name.str())] = build_package;
            }
        }

        // Two pass parsing for profiles
        // First pass: Load all profiles into profiles_ variable
        if (data.contains("profile") && data["profile"].is_table()) {
            for (const auto& [profile_name, profile_data] : *data["profile"].as_table()) {
                if (profile_data.is_table()) {
                    if (profile_data.as_table()->contains("cflags")) {
                        const auto& cflag_array = *profile_data.as_table()->at("cflags").as_array();
                        std::set<std::string> cflags;
                        for (const auto& cflag : cflag_array) {
                            cflags.insert(*cflag.value<std::string>());
                        }
                        profiles_[std::string(profile_name.str())]["cflags"] = cflags;
                        muuk::logger::info("Profile '{}' loaded with {} cflags.", profile_name.str(), cflags.size());
                    }

                    if (profile_data.as_table()->contains("lflags")) {
                        const auto& lflag_array = *profile_data.as_table()->at("lflags").as_array();
                        std::set<std::string> lflags;
                        for (const auto& lflag : lflag_array) {
                            lflags.insert(*lflag.value<std::string>());
                        }
                        profiles_[std::string(profile_name.str())]["lflags"] = lflags;
                        muuk::logger::info("Profile '{}' loaded with {} lflags.", profile_name.str(), lflags.size());
                    }
                }
            }
        }

        // Second pass: Process inheritance
        if (data.contains("profile") && data["profile"].is_table()) {
            for (const auto& [profile_name, profile_data] : *data["profile"].as_table()) {
                if (profile_data.is_table()) {
                    muuk::logger::trace("Parsing profile '{}'", profile_name.str());
                    if (profile_data.as_table()->contains("inherits")) {
                        if (profile_data.as_table()->at("inherits").is_array()) {
                            const auto& inherits_array = *profile_data.as_table()->at("inherits").as_array();
                            for (const auto& inherits : inherits_array) {
                                std::string inherited_profile = *inherits.value<std::string>();
                                if (profiles_.find(inherited_profile) == profiles_.end()) {
                                    muuk::logger::error("Inherited profile '" + inherited_profile + "' not found.");
                                }
                                auto res = merge_profiles(std::string(profile_name.str()), inherited_profile);
                                if (!res) {
                                    muuk::logger::warn("Failed to merge profiles: {}", res.error());
                                }
                            }
                        }
                        else if (profile_data.as_table()->at("inherits").is_string()) {
                            std::string inherited_profile = *profile_data.as_table()->at("inherits").value<std::string>();
                            if (profiles_.find(inherited_profile) == profiles_.end()) {
                                muuk::logger::error("Inherited profile '" + inherited_profile + "' not found.");
                            }
                            auto res = merge_profiles(std::string(profile_name.str()), inherited_profile);
                            if (!res) {
                                muuk::logger::warn("Failed to merge profiles: {}", res.error());
                            }
                        }
                    }
                }
            }
        }

        // if (data.contains("platform") && data["platform"].is_table()) {
        //     for (const auto& [platform_name, platform_data] : *data["platform"].as_table()) {
        //         if (platform_data.is_table() && platform_data.as_table()->contains("cflags")) {
        //             const auto& cflag_array = *platform_data.as_table()->at("cflags").as_array();
        //             std::set<std::string> cflags;
        //             for (const auto& cflag : cflag_array) {
        //                 cflags.insert(*cflag.value<std::string>());
        //             }

        //             package->platform_[std::string(platform_name.str())]["cflags"] = cflags;

        //             muuk::logger::info("Platform '{}' loaded with {} cflags.", platform_name.str(), cflags.size());
        //         }
        //     }
        // }

        // if (data.contains("compiler") && data["compiler"].is_table()) {
        //     for (const auto& [compiler_name, compiler_data] : *data["compiler"].as_table()) {
        //         if (compiler_data.is_table() && compiler_data.as_table()->contains("cflags")) {
        //             const auto& cflag_array = *compiler_data.as_table()->at("cflags").as_array();
        //             std::set<std::string> cflags;
        //             for (const auto& cflag : cflag_array) {
        //                 cflags.insert(*cflag.value<std::string>());
        //             }

        //             package->compiler_[std::string(compiler_name.str())]["cflags"] = cflags;

        //             muuk::logger::info("Compiler '{}' loaded with {} cflags.", compiler_name.str(), cflags.size());
        //         }
        //     }
        // }

    } // if (is_base)

    return {};

}

void MuukLockGenerator::parse_section(const toml::table& section, Package& package) {
    muuk::logger::trace("Parsing section for package: {}", package.name);

    if (section.contains("include")) {
        for (const auto& inc : *section["include"].as_array()) {
            package.add_include_path(*inc.value<std::string>());
            muuk::logger::debug(" Added include path: {}", *inc.value<std::string>());
        }
    }

    if (section.contains("sources")) {
        for (const auto& src : *section["sources"].as_array()) {
            std::string source_entry = *src.value<std::string>();

            std::vector<std::string> extracted_cflags;
            std::string file_path;

            // Splitting "source.cpp CFLAGS" into file_path and flags
            size_t space_pos = source_entry.find(' ');
            if (space_pos != std::string::npos) {
                file_path = source_entry.substr(0, space_pos);
                std::string flags_str = source_entry.substr(space_pos + 1);

                std::istringstream flag_stream(flags_str);
                std::string flag;
                while (flag_stream >> flag) {
                    extracted_cflags.push_back(flag);
                }
            }
            else {
                file_path = source_entry;
            }

            package.sources.emplace_back(file_path, extracted_cflags);
            muuk::logger::debug("Added source file: {}, CFLAGS: {}", file_path, join_strings(extracted_cflags, " "));
        }
    }

    if (section.contains("cflags")) {
        for (const auto& flag : *section["cflags"].as_array()) {
            package.cflags.insert(*flag.value<std::string>());
        }
    }

    if (section.contains("dependencies")) {
        muuk::logger::debug("Dependencies of '{}':", package.name);
        for (const auto& [dep_name, dep_value] : *section["dependencies"].as_table()) {
            std::unordered_map<std::string, std::string> dependency_info;

            package.deps.insert(std::string(dep_name.str()));

            if (dep_value.is_table()) {
                for (const auto& [key, val] : *dep_value.as_table()) {
                    if (val.is_string()) {
                        std::string value = *val.value<std::string>();
                        dependency_info[std::string(key.str())] = value;
                    }
                }
            }
            else if (dep_value.is_string()) {
                dependency_info["version"] = *dep_value.value<std::string>();
            }
            else {
                muuk::logger::error("Invalid format for dependency '{}'. Expected a string or table.", std::string(dep_name.str()));
                continue;
            }

            package.dependencies[std::string(dep_name.str())] = dependency_info;

            muuk::logger::debug("  → Dependency '{}' added with details:", std::string(dep_name.str()));
            for (const auto& [key, val] : dependency_info) {
                muuk::logger::debug("      - {}: {}", key, val);
            }
        }
    }

    if (section.contains("libs")) {
        for (const auto& lib : *section["libs"].as_array()) {
            package.libs.push_back(*lib.value<std::string>());
            muuk::logger::debug(" Added library: {}", *lib.value<std::string>());
        }
    }

    std::vector<std::string> module_paths;
    if (section.contains("modules")) {
        for (const auto& mod : *section["modules"].as_array()) {
            module_paths.push_back(*mod.value<std::string>());
            muuk::logger::info("  Found module source: {}", *mod.value<std::string>());
        }
    }

    if (!module_paths.empty()) {
        process_modules(module_paths, package);
    }
}

tl::expected<void, std::string>
MuukLockGenerator::resolve_dependencies(
    const std::string& package_name,
    std::optional<std::string> search_path
) {
    if (visited.count(package_name)) {
        return tl::unexpected("Circular dependency detected for '" + package_name + "'.");
    }

    visited.insert(package_name);
    muuk::logger::info("Resolving dependencies for: {} with muuk path: '{}'", package_name, search_path.value_or(""));

    // Use helper function to get the package
    auto package_result = get_package(package_name, search_path);
    if (!package_result) {
        return tl::unexpected(package_result.error());
    }

    auto package = package_result.value(); // Safe since we checked `expected`

    for (const auto& [dep_name, dep_info] : package->dependencies) {
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
            auto sys_dep_result = resolve_system_dependency(dep_name);
            if (!sys_dep_result) {
                muuk::logger::error("Failed to resolve system dependency '{}': {}", dep_name, sys_dep_result.error());
                return tl::unexpected(sys_dep_result.error());
            }
        }
        else {
            auto dep_result = resolve_dependencies(dep_name, dep_search_path.empty() ? std::nullopt : std::optional<std::string>{ dep_search_path });
            if (!dep_result) {
                return tl::unexpected(dep_result.error());
            }
        }

        if (resolved_packages_["library"].count(dep_name)) {
            muuk::logger::info("Merging '{}' into '{}'", dep_name, package_name);
            package->merge(*resolved_packages_["library"][dep_name]);
        }

        toml::table dependency_entry;
        for (const auto& [key, value] : dep_info) {
            dependency_entry.insert(key, value);
        }

        dependencies_[dep_name] = dependency_entry;
    }

    muuk::logger::info("Added '{}' to resolved order list.", package_name);
    resolved_order_.push_back(package_name);

    return {};
}

tl::expected<std::shared_ptr<Package>, std::string>
MuukLockGenerator::get_package(
    const std::string& package_name,
    std::optional<std::string> search_path
) {
    // Check if package already exists
    if (resolved_packages_["library"].count(package_name)) {
        return resolved_packages_["library"][package_name];
    }

    if (resolved_packages_["build"].count(package_name)) {
        return resolved_packages_["build"][package_name];
    }

    // If no explicit search path, look in dependency folders
    if (!search_path) {
        search_and_parse_dependency(package_name);
        if (resolved_packages_["library"].count(package_name)) {
            return resolved_packages_["library"][package_name];
        }
        return tl::unexpected("Package '" + package_name + "' not found in dependency folders.");
    }

    // If a search path is given, try parsing it
    fs::path search_file = fs::path(*search_path);
    if (!search_path->ends_with("muuk.toml")) {
        search_file /= "muuk.toml";
    }

    if (!fs::exists(search_file)) {
        return tl::unexpected("Search path '" + search_file.string() + "' does not exist.");
    }

    auto parse_result = parse_muuk_toml(search_file.string());
    if (!parse_result) {
        return tl::unexpected("Failed to parse dependency '" + package_name + "': " + parse_result.error());
    }

    if (resolved_packages_["library"].count(package_name)) {
        return resolved_packages_["library"][package_name];
    }

    return tl::unexpected("Package '" + package_name + "' not found after parsing '" + search_file.string() + "'.");
}


// Searches for the specified package in the dependency folder and parses its muuk.toml file if found.
void MuukLockGenerator::search_and_parse_dependency(const std::string& package_name) {
    muuk::logger::info("Searching for target package '{}'.", package_name);
    fs::path modules_dir = fs::path(DEPENDENCY_FOLDER);

    if (!fs::exists(modules_dir)) return;

    for (const auto& dir_entry : fs::directory_iterator(modules_dir)) {
        if (dir_entry.is_directory() && dir_entry.path().filename().string().find(package_name) != std::string::npos) {
            fs::path dep_path = dir_entry.path() / "muuk.toml";
            if (fs::exists(dep_path)) {
                auto parse_result = parse_muuk_toml(dep_path.string());
                if (!parse_result) {
                    muuk::logger::error("Error parsing muuk.toml: {}", parse_result.error());
                    return;  // Stop execution instead of proceeding with invalid state.
                }

                return;
            }
        }
    }
    muuk::logger::warn("Dependency '{}' not found in '{}'", package_name, modules_dir.string());
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

    auto parse_result = parse_muuk_toml(base_path_ + "muuk.toml", true);
    if (!parse_result) {
        muuk::logger::error(
            "Failed to parse {}/muuk.toml: {}",
            base_path_,
            parse_result.error()
        );
        return;
    }

    muuk::logger::info("Resolving dependencies for build packages...");
    for (const auto& [build_name, _] : resolved_packages_["build"]) {
        auto result = resolve_dependencies(build_name);
        if (!result) {
            muuk::logger::error("Failed to resolve dependencies for '{}': {}", build_name, result.error());
            return;
        }
    }

    for (const auto& package_name : resolved_order_) {
        auto package_opt = find_package(package_name);
        if (!package_opt) continue;

        auto package = package_opt.value();
        std::string package_type = package->package_type;

        std::string package_table = package->serialize();

        lockfile << "[" << package_type << "." << package_name << "]\n";
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
std::optional<std::shared_ptr<Package>> MuukLockGenerator::find_package(const std::string& package_name) {
    if (resolved_packages_["library"].count(package_name)) {
        return resolved_packages_["library"][package_name];
    }

    if (resolved_packages_["build"].count(package_name)) {
        return resolved_packages_["build"][package_name];
    }
    return std::nullopt;
}

// Processes the given module paths and adds the resolved modules to the package.
void MuukLockGenerator::process_modules(const std::vector<std::string>& module_paths, Package& package) {
    muuk::logger::info("Processing modules for package: {}", package.name);

    module_parser_->parseAllModules(module_paths);

    std::vector<std::string> resolved_modules = module_parser_->resolveAllModules();

    for (const auto& mod : resolved_modules) {
        package.modules.push_back(mod);
        muuk::logger::info("Resolved and added module '{}' to package '{}'", mod, package.name);
    }
}

// Resolves system dependencies by checking for include and library paths for the given package name.
tl::expected<void, std::string>
MuukLockGenerator::resolve_system_dependency(const std::string& package_name) {
    muuk::logger::info("Checking system dependency: '{}'", package_name);

    std::string include_path, lib_path;

#ifdef _WIN32
    return tl::unexpected("System dependency resolution on Windows is not fully automated.");
#else
    include_path = util::execute_command("pkg-config --cflags-only-I " + package_name + " | sed 's/-I//' | tr -d '\n'");
    lib_path = util::execute_command("pkg-config --libs-only-L " + package_name + " | sed 's/-L//' | tr -d '\n'");
#endif

    if (include_path.empty() || !util::path_exists(include_path)) {
        return tl::unexpected("Include path for '" + package_name + "' not found.");
    }

    if (lib_path.empty() || !fs::exists(lib_path)) {
        return tl::unexpected("Library path for '" + package_name + "' not found.");
    }

    return {};
}

// Merges the settings of the inherited profile into the base profile.
tl::expected<void, std::string>
MuukLockGenerator::merge_profiles(
    const std::string& base_profile,
    const std::string& inherited_profile
) {
    if (profiles_.find(inherited_profile) == profiles_.end()) {
        return tl::unexpected("Inherited profile '" + inherited_profile + "' not found.");
    }

    muuk::logger::trace("Merging profile '{}' into '{}'", inherited_profile, base_profile);

    for (const auto& [key, values] : profiles_[inherited_profile]) {
        profiles_[base_profile][key].insert(values.begin(), values.end());
    }
}