#include <memory>
#include <string>

#include <fmt/ranges.h>
#include <toml.hpp>

#include "logger.h"
#include "muuklockgen.h"
#include "rustify.hpp"

Result<void> MuukLockGenerator::parse_dependencies(const toml::value& data, std::shared_ptr<Package> package) {

    if (data.contains("dependencies") && data.at("dependencies").is_table()) {
        for (const auto& [dep_name, dep_value] : data.at("dependencies").as_table()) {
            auto dep_entry = std::make_shared<Dependency>();

            auto dep_result = dep_entry->load(dep_name, dep_value);
            if (!dep_result)
                return Err(dep_result);

            // Store in our internal maps
            auto& existing_entry = dependencies_[dep_name][dep_entry->version];
            if (!existing_entry) {
                // Only use the newly parsed entry if it doesn't exist
                existing_entry = dep_entry;
            } else {
                // Merge features if already present
                existing_entry->enabled_features.insert(
                    dep_entry->enabled_features.begin(),
                    dep_entry->enabled_features.end());
            }

            // Also add it to the package
            package->dependencies_[dep_name][dep_entry->version] = dependencies_[dep_name][dep_entry->version];

            muuk::logger::info("  → Dependency '{}' (v{}) added with details:", dep_name, dep_entry->version);
        }
    }

    return {};
}

Result<void> MuukLockGenerator::parse_profile(const toml::value& data) {

    // Two pass parsing for profiles
    // First pass: Load all profiles into profiles_ variable
    if (data.contains("profile") && data.at("profile").is_table())
        parse_and_store_flag_categories(
            data.at("profile").as_table(),
            profiles_,
            { "cflags", "lflags", "defines" });

    // Second pass: Process inheritance
    if (data.contains("profile") && data.at("profile").is_table())
        for (const auto& [profile_name, profile_data] : data.at("profile").as_table()) {
            if (profile_data.is_table()) {
                muuk::logger::trace("Parsing profile '{}'", profile_name);
                if (profile_data.contains("inherits")) {
                    if (profile_data.at("inherits").is_array()) {
                        const auto& inherits_array = profile_data.at("inherits").as_array();
                        for (const auto& inherits : inherits_array) {
                            std::string inherited_profile = inherits.as_string();
                            if (profiles_.find(inherited_profile) == profiles_.end()) {
                                muuk::logger::error("Inherited profile '" + inherited_profile + "' not found.");
                                return Err("");
                            }
                            merge_profiles(profile_name, inherited_profile);
                        }
                    } else if (profile_data.as_table().at("inherits").is_string()) {
                        std::string inherited_profile = profile_data.as_table().at("inherits").as_string();
                        if (profiles_.find(inherited_profile) == profiles_.end()) {
                            muuk::logger::error("Inherited profile '" + inherited_profile + "' not found.");
                            return Err("");
                        }
                        merge_profiles(profile_name, inherited_profile);
                    }
                }
            }
        }

    return {};
}

Result<void> MuukLockGenerator::parse_features(const toml::value& data, std::shared_ptr<Package> package) {
    if (!data.contains("features") || !data.at("features").is_table()) {
        muuk::logger::info("No 'features' section found in TOML.");
        return {};
    }

    for (const auto& [feature_name, feature_value] : data.at("features").as_table()) {
        Feature feature_data;

        if (feature_value.is_array()) { // List Syntax
            for (const auto& item : feature_value.as_array()) {
                if (item.is_string()) {
                    std::string value = item.as_string();

                    if (value.rfind("D:", 0) == 0) {
                        feature_data.defines.insert(value.substr(2)); // Remove "D:"
                    } else if (value.rfind("dep:", 0) == 0) {
                        feature_data.dependencies.insert(value.substr(4)); // Remove "dep:"
                    } else {
                        std::cerr << "Warning: Unrecognized feature syntax: " << value << "\n";
                    }
                }
            }
        } else if (feature_value.is_table()) { // Table Syntax

            if (feature_value.contains("define") && feature_value.at("define").is_array()) {
                for (const auto& def : feature_value.at("define").as_array()) {
                    if (def.is_string()) {
                        feature_data.defines.insert(def.as_string());
                    }
                }
            }

            if (feature_value.contains("dependencies") && feature_value.at("dependencies").is_array()) {
                for (const auto& dep : feature_value.at("dependencies").as_array()) {
                    if (dep.is_string()) {
                        feature_data.dependencies.insert(dep.as_string());
                    }
                }
            }
        } else {
            // TODO: This eventually will be unnecessary
            muuk::logger::warn("Invalid format for feature '{}'. Must be either a table or a array", std::string(feature_name));
            continue;
        }

        package->features[std::string(feature_name)] = feature_data;
    }
    return {};
}