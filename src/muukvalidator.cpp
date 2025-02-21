#include "logger.h"
#include "muukvalidator.hpp"

#include <toml++/toml.h>
#include <iostream>
#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>


namespace muuk {

    TomlType get_toml_type(const toml::node& node) {
        if (node.is_table()) return TomlType::Table;
        if (node.is_array()) return TomlType::Array;
        if (node.is_string()) return TomlType::String;
        if (node.is_integer()) return TomlType::Integer;
        if (node.is_floating_point()) return TomlType::Float;
        if (node.is_boolean()) return TomlType::Boolean;
        if (node.is_date()) return TomlType::Date;
        if (node.is_time()) return TomlType::Time;
        if (node.is_date_time()) return TomlType::DateTime;
        throw invalid_toml("Unknown TOML type");
    }

    bool validate_toml_(
        const toml::table& toml_data,
        const std::unordered_map<std::string, SchemaNode>& schema,
        std::string parent_path
    ) {
        for (const auto& [key, schema_node] : schema) {
            std::string full_path = parent_path.empty() ? key : parent_path + "." + key;

            // Check if key exists
            if (!toml_data.contains(key)) {
                if (schema_node.required) {
                    throw invalid_toml("Missing required key: " + full_path);
                }
                continue;
            }

            // Get TOML node
            const toml::node& node = *toml_data.get(key);
            TomlType node_type = get_toml_type(node);

            // Validate type
            if (node_type != schema_node.type) {
                std::cerr << "Type mismatch at " << full_path << "\n";
                throw invalid_toml("Type mismatch at " + full_path);
            }

            // Handle array validation
            if (schema_node.type == TomlType::Array && schema_node.allowed_types) {
                auto arr = node.as_array();
                if (!schema_node.allowed_types->empty()) {
                    TomlType first_type = get_toml_type(*arr->get(0));
                    for (const auto& item : *arr) {
                        TomlType item_type = get_toml_type(item);
                        if (std::find(schema_node.allowed_types->begin(), schema_node.allowed_types->end(), item_type) == schema_node.allowed_types->end()) {
                            std::cerr << "Array at " << full_path << " contains invalid type\n";
                            throw invalid_toml("Array at " + full_path + " contains invalid type");
                        }
                    }
                }
            }

            if (schema_node.type == TomlType::Table && schema_node.children) {
                auto tbl = node.as_table();
                validate_toml_(*tbl, *schema_node.children, full_path);
            }
        }
        return true;
    }

    bool validate_muuk_toml(const toml::table& toml_data) {
        std::unordered_map<std::string, SchemaNode> schema = std::unordered_map<std::string, SchemaNode>{
            {"package", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                {"name", {true, TomlType::String, std::nullopt, std::nullopt}},
                {"version", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"description", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"license", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"authors", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"repository", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"documentation", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"homepage", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"readme", {false, TomlType::String, std::nullopt, std::nullopt}},
                {"keywords", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"library", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                {"include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"system_include", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"dependencies", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                }}},
                {"compiler", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                    {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
                }}},
                {"platform", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                    {"cflags", {false, TomlType::Array,std::vector<TomlType>{TomlType::String}, std::nullopt}}
                }}}
            }}},
            {"build", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
            }}},
            {"profile", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                {"default", {false, TomlType::Boolean, std::nullopt, std::nullopt}},
                {"inherits", {false, TomlType::String, std::nullopt, std::nullopt}}
            }}},
            {"platform", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}},
                {"lflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}},
            {"compiler", {false, TomlType::Table, std::nullopt, std::unordered_map<std::string, SchemaNode>{
                {"cflags", {false, TomlType::Array, std::vector<TomlType>{TomlType::String}, std::nullopt}}
            }}}
        };

        return validate_toml_(toml_data, schema);
    };
} // namespace muuk
