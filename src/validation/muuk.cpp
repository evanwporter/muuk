#include "logger.h"
#include "muukvalidator.hpp"
#include "rustify.hpp"

#include <toml++/toml.h>
#include <iostream>
#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>

namespace muuk {

    Result<TomlType> get_toml_type(const toml::node& node) {
        if (node.is_table()) return TomlType::Table;
        if (node.is_array()) return TomlType::Array;
        if (node.is_string()) return TomlType::String;
        if (node.is_integer()) return TomlType::Integer;
        if (node.is_floating_point()) return TomlType::Float;
        if (node.is_boolean()) return TomlType::Boolean;
        if (node.is_date()) return TomlType::Date;
        if (node.is_time()) return TomlType::Time;
        if (node.is_date_time()) return TomlType::DateTime;

        return Err("Unknown TOML type");
    }

    Result<void> validate_toml_(
        const toml::table& toml_data,
        const SchemaMap& schema,
        std::string parent_path
    ) {
        bool has_wildcard = schema.contains("*");
        const SchemaNode* wildcard_schema = has_wildcard ? &schema.at("*") : nullptr;

        for (const auto& [key, schema_node] : schema) {
            if (key == "*") continue; // Wildcard handled separately

            std::string full_path = parent_path.empty() ? std::string(key) : parent_path + "." + std::string(key);

            // Check if key exists
            if (!toml_data.contains(key)) {
                if (schema_node.required) {
                    return Err("Missing required key: {}", full_path);
                }
                continue;
            }

            // Validate type
            const toml::node& node = *toml_data.get(key);
            auto node_type = get_toml_type(node);
            if (!node_type) {
                return Err(node_type.error());
            }

            if (*node_type != schema_node.type) {
                return Err("Type mismatch at {}", full_path);
            }

            // Handle array validation
            if (schema_node.type == TomlType::Array && schema_node.allowed_types) {
                auto arr = node.as_array();
                if (!arr || arr->empty()) {
                    return Err("Array at {} is empty or invalid.", full_path);
                }

                for (const auto& item : *arr) {
                    auto item_type = get_toml_type(item);
                    if (!item_type) {
                        return Err("Invalid array element type at {}", full_path);
                    }
                    if (std::find(schema_node.allowed_types->begin(), schema_node.allowed_types->end(), *item_type) == schema_node.allowed_types->end()) {
                        return Err("Array at {} contains invalid type", full_path);
                    }
                }
            }

            // Recursively validate nested tables
            if (schema_node.type == TomlType::Table && schema_node.children) {
                auto tbl = node.as_table();
                if (!tbl) {
                    return Err("Invalid table at {}", full_path);
                }
                auto result = validate_toml_(*tbl, *schema_node.children, full_path);
                if (!result) {
                    return result;
                }
            }
        }

        // Handle wildcard schema
        if (has_wildcard) {
            for (const auto& [key, node] : toml_data) {
                std::string full_path = parent_path.empty() ? std::string(key) : parent_path + "." + std::string(key);

                // Validate type
                auto node_type = get_toml_type(node);
                if (!node_type) {
                    return Err(node_type.error());
                }

                if (*node_type != wildcard_schema->type) {
                    return Err("Type mismatch at {}", full_path);
                }

                // Recursively validate wildcard children
                if (wildcard_schema->type == TomlType::Table && wildcard_schema->children) {
                    auto tbl = node.as_table();
                    if (!tbl) {
                        return Err("Invalid table at {}", full_path);
                    }
                    auto result = validate_toml_(*tbl, *wildcard_schema->children, full_path);
                    if (!result) {
                        return result;
                    }
                }
            }
        }

        return {}; // Success
    }

    Result<void> validate_muuk_toml(const toml::table& toml_data) {
        SchemaMap schema = SchemaMap{
            {"package", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"name", {true, TomlType::String, std::nullopt, std::nullopt}},
                {"version", {true, TomlType::String, std::nullopt, std::nullopt}},
                {"description", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"license", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"authors", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"repository", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"documentation", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"homepage", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"readme", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"keywords", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"dependencies", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"*", {false, TomlType::Table, std::nullopt, SchemaMap{
                    {"git", {false, TomlType::String, std::nullopt, std::nullopt}},
                    {"muuk_path", {false, TomlType::String, std::nullopt, std::nullopt}},
                    {"version", {true, TomlType::String, std::nullopt, std::nullopt}}
                }}}
            }}},
            {"library", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"source", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"libflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"lflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"system_include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"build", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"*", {false, TomlType::Table, std::nullopt, SchemaMap{
                    {"include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"system_include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"dependencies", {false, TomlType::Table, std::nullopt, SchemaMap{
                    }}}
                }}}
            }}},
            {"profile", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"default", {false, TomlType::Boolean, std::nullopt, std::nullopt}},
                {"inherits", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"lflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"platform", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"lflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"compiler", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"lflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}}
        };

        return validate_toml_(toml_data, schema);
    };

    Result<void> validate_muuk_lock_toml(const toml::table& toml_data) {
        SchemaMap schema = SchemaMap{
            {"library", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"*", {false, TomlType::Table, std::nullopt, SchemaMap{
                    {"include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"source", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"system_include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"dependencies", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
                }}}
            }}},

            {"build", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"*", {false, TomlType::Table, std::nullopt, SchemaMap{
                    {"include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"system_include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                    {"dependencies", {false, TomlType::Table, std::nullopt, SchemaMap{
                    }}}
                }}}
            }}},

            {"dependencies", {false, TomlType::Table, std::nullopt, SchemaMap{
                {"*", {false, TomlType::Table, std::nullopt, SchemaMap{
                    {"git", {true, TomlType::String, std::nullopt, std::nullopt}},
                    {"muuk_path", {true, TomlType::String, std::nullopt, std::nullopt}},
                    {"version", {true, TomlType::String, std::nullopt, std::nullopt}}
                }}}
            }}}
        };

        return {};//validate_toml_(toml_data, schema);
    }

} // namespace muuk
