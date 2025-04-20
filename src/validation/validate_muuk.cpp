#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <toml.hpp>

#include "muuk_schema.hpp"
#include "muukvalidator.hpp"
#include "rustify.hpp"

namespace muuk {

    Result<TomlType> get_toml_type(const toml::value& node) {
        if (node.is_table())
            return TomlType::Table;
        if (node.is_array())
            return TomlType::Array;
        if (node.is_string())
            return TomlType::String;
        if (node.is_integer())
            return TomlType::Integer;
        if (node.is_floating())
            return TomlType::Float;
        if (node.is_boolean())
            return TomlType::Boolean;
        if (node.is_local_date())
            return TomlType::Date;
        if (node.is_local_time())
            return TomlType::Time;
        if (node.is_local_datetime() || node.is_offset_datetime())
            return TomlType::DateTime;

        return Err("Unknown TOML type");
    }

    inline std::string to_string(TomlType type) {
        switch (type) {
        case TomlType::Table:
            return "Table";
        case TomlType::Array:
            return "Array";
        case TomlType::String:
            return "String";
        case TomlType::Integer:
            return "Integer";
        case TomlType::Float:
            return "Float";
        case TomlType::Boolean:
            return "Boolean";
        case TomlType::Date:
            return "Date";
        case TomlType::Time:
            return "Time";
        case TomlType::DateTime:
            return "DateTime";
        default:
            return "Unknown";
        }
    }

    Result<void> validate_toml_(const toml::value& toml_data, const SchemaMap& schema, const std::string& parent_path = "") {
        bool has_wildcard = schema.contains("*");
        const SchemaNode* wildcard_schema = has_wildcard ? &schema.at("*") : nullptr;

        for (const auto& [key, schema_node] : schema) {
            if (key == "*")
                continue; // Wildcards handled separately

            std::string full_path = parent_path.empty() ? key : parent_path + "." + key;

            if (!toml_data.contains(key)) {
                if (schema_node.required)
                    return Err("Missing required key: {}", full_path);
                continue;
            }

            const auto& node = toml_data.at(key);
            auto node_type = get_toml_type(node);
            if (!node_type)
                return Err(toml::format_error(
                    "Could not determine type",
                    node,
                    node_type.error()));

            if (std::holds_alternative<TomlType>(schema_node.type)) {
                TomlType expected = std::get<TomlType>(schema_node.type);
                if (*node_type != expected)
                    return Err(toml::format_error(
                        "Validation failed due to a type mismatch",
                        node,
                        "Expected type: " + to_string(expected) + ", but got: " + to_string(*node_type)));

            } else if (std::holds_alternative<TomlArray>(schema_node.type)) {
                if (*node_type != TomlType::Array)
                    return Err(toml::format_error(
                        "Validation failed due to a type mismatch",
                        node,
                        "Expected an array at '" + full_path + "'"));

                const auto& arr = node.as_array();
                const TomlArray& expected_array = std::get<TomlArray>(schema_node.type);

                for (const auto& item : arr) {
                    auto item_type = get_toml_type(item);
                    if (!item_type || *item_type != expected_array.element_type)
                        return Err(toml::format_error(
                            "Validation failed due to a array item type mismatch",
                            item,
                            "Expected element type: " + to_string(expected_array.element_type) + ", but got: " + to_string(*item_type)));

                    // If it's an array of tables, validate each table
                    if (expected_array.element_type == TomlType::Table && expected_array.table_schema)
                        if (!item.is_table())
                            return Err(toml::format_error(
                                "Validation failed due to a type mismatch",
                                item,
                                "Expected a table in array at '" + full_path + "'"));
                }
            }

            if (node.is_table() && !schema_node.children.empty()) {
                auto result = validate_toml_(node, schema_node.children, full_path);
                if (!result)
                    return result;
            }
        }

        // Handle wildcard keys
        if (has_wildcard) {
            const auto& tbl = toml_data.as_table();

            for (const auto& [k, node] : tbl) {
                std::string key_str = k;
                if (schema.contains(key_str))
                    continue;

                std::string full_path = parent_path.empty() ? key_str : parent_path + "." + key_str;
                auto node_type = get_toml_type(node);
                if (!node_type)
                    return Err(toml::format_error(
                        "Unknown type",
                        node,
                        node_type.error()));

                bool wildcard_match = std::visit([&, node = node](auto&& expected) -> bool {
                    using T = std::decay_t<decltype(expected)>;

                    if constexpr (std::is_same_v<T, TomlType>) {
                        return *node_type == expected;
                    } else if constexpr (std::is_same_v<T, TomlArray>) {
                        if (*node_type != TomlType::Array)
                            return false;
                        const auto& arr = node.as_array();
                        for (const auto& item : arr) {
                            auto item_type = get_toml_type(item);
                            if (!item_type || *item_type != expected.element_type) {
                                return false;
                            }
                        }
                        return true;
                    } else if constexpr (std::is_same_v<T, std::vector<TomlType>>) {
                        return std::find(expected.begin(), expected.end(), *node_type) != expected.end();
                    }
                    // clang-format off
                }, wildcard_schema->type);

                if (!wildcard_match) 
                    return Err(toml::format_error(
                        "Validation failed due to a type mismatch",
                        node,
                        "Type mismatch at key '" + full_path + "'"
                    ));

                if (node.is_table() && !wildcard_schema->children.empty()) {
                    auto result = validate_toml_(node, wildcard_schema->children, full_path);
                    if (!result)
                        return result;
                }
            }
        }
        return {}; // Success
    }

    Result<void> validate_muuk_toml(const toml::value& toml_data) {
        return validate_toml_(toml_data, muuk_schema);
    };

    Result<void> validate_muuk_lock_toml(const toml::value& toml_data) {
        (void)toml_data;
        return {};
        // return validate_toml_(toml_data, muuk_lock_schema);
    }

} // namespace muuk
