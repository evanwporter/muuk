#include "muuklockgen.h"
#include "logger.h"

#include <toml++/toml.h>
#include <fmt/ranges.h>


Result<void> MuukLockGenerator::parse_dependencies(const toml::table& data, std::shared_ptr<Component> package) {

    if (data.contains("dependencies") && data["dependencies"].is_table()) {
        for (const auto& [dep_name, dep_value] : *data["dependencies"].as_table()) {
            std::unordered_map<std::string, std::string> dependency_info;
            std::string version = "unknown";

            package->deps.insert(std::string(dep_name.str()));

            if (dep_value.is_table()) {
                for (const auto& [key, val] : *dep_value.as_table()) {
                    if (val.is_string()) {
                        std::string key_ = std::string(key.str());
                        std::string value = *val.value<std::string>();
                        if (key_ == "version") {
                            version = value;
                        }
                        else {
                            dependency_info[key_] = value;
                        }
                    }
                }
            }
            else if (dep_value.is_string()) { // Ensure it's a versioning string
                version = *dep_value.value<std::string>();
            }
            else {
                muuk::logger::error("Invalid format for dependency '{}'. Expected a string or table.", std::string(dep_name.str()));
                continue;
            }

            package->dependencies_[std::string(dep_name.str())][version] = dependency_info;

            muuk::logger::info("  → Dependency '{}' (v{}) added with details:", std::string(dep_name.str()), version);
            for (const auto& [key, val] : dependency_info) {
                muuk::logger::info("      - {}: {}", key, val);
            }
        }
    }

    return {};
}


Result<void> MuukLockGenerator::parse_library(const toml::table& data, std::shared_ptr<Component> package) {
    if (data.contains("library") && data["library"].is_table()) {

        muuk::logger::trace("Parsing section for package: {}", package->name);

        const auto& section = *data["library"].as_table();

        if (section.contains("include")) {
            for (const auto& inc : *section["include"].as_array()) {
                package->add_include_path(*inc.value<std::string>());
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

                package->sources.emplace_back(file_path, extracted_cflags);
                muuk::logger::debug("Added source file: {}, CFLAGS: {}", file_path, fmt::join(extracted_cflags, " "));
            }
        }

        if (section.contains("cflags")) {
            for (const auto& flag : *section["cflags"].as_array()) {
                package->cflags.insert(*flag.value<std::string>());
            }
        }

        if (section.contains("libs")) {
            for (const auto& lib : *section["libs"].as_array()) {
                package->libs.push_back(*lib.value<std::string>());
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

        // if (!module_paths.empty()) {
        //     process_modules(module_paths, package);
        // }
    }
    return {};
}

Result<void> MuukLockGenerator::parse_profile(const toml::table& data) {

    // Two pass parsing for profiles
    // First pass: Load all profiles into profiles_ variable
    if (data.contains("profile") && data["profile"].is_table()) {
        for (const auto& [profile_name, profile_data] : *data["profile"].as_table()) {
            if (profile_data.is_table()) {
                if (profile_data.as_table()->contains("cflags")) {
                    const auto& cflag_array = *profile_data.as_table()->at("cflags").as_array();
                    std::unordered_set<std::string> cflags;
                    for (const auto& cflag : cflag_array) {
                        cflags.insert(*cflag.value<std::string>());
                    }
                    profiles_[std::string(profile_name.str())]["cflags"] = cflags;
                    muuk::logger::info("Profile '{}' loaded with {} cflags.", profile_name.str(), cflags.size());
                }

                if (profile_data.as_table()->contains("lflags")) {
                    const auto& lflag_array = *profile_data.as_table()->at("lflags").as_array();
                    std::unordered_set<std::string> lflags;
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
                            merge_profiles(std::string(profile_name.str()), inherited_profile);
                        }
                    }
                    else if (profile_data.as_table()->at("inherits").is_string()) {
                        std::string inherited_profile = *profile_data.as_table()->at("inherits").value<std::string>();
                        if (profiles_.find(inherited_profile) == profiles_.end()) {
                            muuk::logger::error("Inherited profile '" + inherited_profile + "' not found.");
                        }
                        merge_profiles(std::string(profile_name.str()), inherited_profile);
                    }
                }
            }
        }
    }
    return {};
}

Result<void> MuukLockGenerator::parse_builds(const toml::table& data, const std::string& package_version, const std::string& path) {

    if (data.contains("build") && data["build"].is_table()) {
        for (const auto& [build_name, build_info] : *data["build"].as_table()) {
            muuk::logger::info("Found build target: {}", build_name.str());

            auto build_package = std::make_shared<Component>(
                std::string(build_name.str()),
                std::string(package_version),
                fs::path(path).parent_path().string(),
                "build"
            );

            parse_library(*build_info.as_table(), build_package);

            if (build_info.is_table() && build_info.as_table()->contains("profile")) {
                const auto& profile_array = *build_info.as_table()->at("profile").as_array();
                for (const auto& profile : profile_array) {
                    build_package->inherited_profiles.insert(*profile.value<std::string>());
                    muuk::logger::info(fmt::format("  → Build '{}' inherits profile '{}'", build_name.str(), *profile.value<std::string>()));
                }
            }

            builds[std::string(build_name.str())] = build_package;

        }
    }
    return {};
}

Result<void> MuukLockGenerator::parse_platform(const toml::table& data, std::shared_ptr<Component> package) {
    if (data.contains("platform") && data["platform"].is_table()) {
        for (const auto& [platform_name, platform_data] : *data["platform"].as_table()) {
            if (platform_data.is_table() && platform_data.as_table()->contains("cflags")) {
                const auto& cflag_array = *platform_data.as_table()->at("cflags").as_array();
                std::unordered_set<std::string> cflags;
                for (const auto& cflag : cflag_array) {
                    cflags.insert(*cflag.value<std::string>());
                }

                package->platform_[std::string(platform_name.str())]["cflags"] = cflags;

                muuk::logger::info("Platform '{}' loaded with {} cflags.", platform_name.str(), cflags.size());
            }
        }
    }
    return {};
}

Result<void> MuukLockGenerator::parse_compiler(const toml::table& data, std::shared_ptr<Component> package) {
    if (data.contains("compiler") && data["compiler"].is_table()) {
        for (const auto& [compiler_name, compiler_data] : *data["compiler"].as_table()) {
            if (compiler_data.is_table() && compiler_data.as_table()->contains("cflags")) {
                const auto& cflag_array = *compiler_data.as_table()->at("cflags").as_array();
                std::unordered_set<std::string> cflags;
                for (const auto& cflag : cflag_array) {
                    cflags.insert(*cflag.value<std::string>());
                }

                package->compiler_[std::string(compiler_name.str())]["cflags"] = cflags;

                muuk::logger::info("Compiler '{}' loaded with {} cflags.", compiler_name.str(), cflags.size());
            }
        }
    }
    return {};
}