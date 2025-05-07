#include <memory>
#include <string>

#include <fmt/ranges.h>
#include <toml.hpp>

#include "lockgen/muuklockgen.hpp"
#include "logger.hpp"
#include "rustify.hpp"

namespace muuk {
    namespace lockgen {
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
            // First pass: Load all profiles
            if (data.contains("profile") && data.at("profile").is_table()) {
                for (const auto& [profile_name, profile_data] : data.at("profile").as_table()) {
                    if (!profile_data.is_table())
                        continue;

                    ProfileConfig config;
                    config.name = profile_name;
                    config.load(profile_data, profile_name, base_path_); // assuming you have `base_path_`
                    profiles_config_[profile_name] = std::move(config);
                }
            }

            // Second pass: Process inheritance
            if (data.contains("profile") && data.at("profile").is_table()) {
                for (const auto& [profile_name, profile_data] : data.at("profile").as_table()) {
                    if (!profile_data.is_table())
                        continue;

                    muuk::logger::trace("Parsing inheritance for profile '{}'", profile_name);

                    if (profile_data.contains("inherits")) {
                        auto& current_profile = profiles_config_.at(profile_name);

                        if (profile_data.at("inherits").is_array()) {
                            for (const auto& inherits : profile_data.at("inherits").as_array()) {
                                std::string inherited_profile = inherits.as_string();

                                if (profiles_config_.find(inherited_profile) == profiles_config_.end())
                                    return Err("Inherited profile '{}' not found.", inherited_profile);

                                current_profile.merge(profiles_config_.at(inherited_profile));
                            }
                        } else if (profile_data.at("inherits").is_string()) {
                            std::string inherited_profile = profile_data.at("inherits").as_string();

                            if (profiles_config_.find(inherited_profile) == profiles_config_.end())
                                return Err("Inherited profile '{}' not found.", inherited_profile);

                            current_profile.merge(profiles_config_.at(inherited_profile));
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

                // handle [features].default
                if (feature_name == "default") {
                    if (!feature_value.is_array()) {
                        muuk::logger::warn("Feature 'default' must be an array of strings.");
                        continue;
                    }

                    for (const auto& default_feat : feature_value.as_array()) {
                        if (default_feat.is_string()) {
                            package->default_features.insert(default_feat.as_string());
                            muuk::logger::info(" → Default feature enabled: {}", default_feat.as_string());
                        }
                    }
                    continue;
                }

                if (feature_value.is_array()) { // List Syntax
                    for (const auto& item : feature_value.as_array()) {
                        std::string value = item.as_string();

                        if (value.rfind("D:", 0) == 0)
                            feature_data.defines.insert(value.substr(2)); // Remove "D:"
                        else if (value.rfind("U:", 0) == 0)
                            feature_data.undefines.insert(value.substr(2)); // Remove "U:"
                        else if (value.rfind("dep:", 0) == 0)
                            feature_data.dependencies.insert(value.substr(4)); // Remove "dep:"
                        else
                            muuk::logger::warn("Unrecognized feature syntax: {}", value);
                    }
                } else if (feature_value.is_table()) { // Table Syntax

                    if (feature_value.contains("define") && feature_value.at("define").is_array())
                        for (const auto& def : feature_value.at("define").as_array())
                            if (def.is_string())
                                feature_data.defines.insert(def.as_string());

                    if (feature_value.contains("dependencies") && feature_value.at("dependencies").is_array())
                        for (const auto& dep : feature_value.at("dependencies").as_array())
                            if (dep.is_string())
                                feature_data.dependencies.insert(dep.as_string());

                } else {
                    // TODO: This eventually will be unnecessary
                    muuk::logger::warn("Invalid format for feature '{}'. Must be either a table or a array", std::string(feature_name));
                    continue;
                }

                package->features[std::string(feature_name)] = feature_data;
            }

            // Validate all default features exist
            for (const std::string& feat : package->default_features)
                if (package->features.find(feat) == package->features.end())
                    muuk::logger::warn("Default feature '{}' is not defined in the [features] table.", feat);

            return {};
        }
    } // namespace lockgen
} // namespace muuk