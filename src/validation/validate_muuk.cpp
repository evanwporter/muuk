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

    // clang-format off

    /// Helper to match a TOML value against a schema variant
    bool match_type(const toml::value& node, const TomlTypeVariantOneType& expected_type, const std::string& path, std::function<Result<void>(const toml::value&, const SchemaMap&, const std::string&)> recurse) {
        auto node_type = get_toml_type(node);
        if (!node_type) return false;

        return std::visit([&](auto&& expected) -> bool {
            using T = std::decay_t<decltype(expected)>;

            if constexpr (std::is_same_v<T, TomlType>) {
                return *node_type == expected;
            } else if constexpr (std::is_same_v<T, TomlArray>) {
                if (*node_type != TomlType::Array) return false;

                const auto& arr = node.as_array();
                for (const auto& item : arr) {
                    auto item_type = get_toml_type(item);
                    if (!item_type || *item_type != expected.element_type) return false;

                    if (expected.element_type == TomlType::Table && expected.table_schema) {
                        if (!item.is_table()) return false;
                        auto result = recurse(item, *expected.table_schema, path + "[]");
                        if (!result) return false;
                    }
                }
                return true;
            }
        }, expected_type);
    }

    /// Function to validate TOML data against a schema
    Result<void> validate_toml_(const toml::value& toml_data, const SchemaMap& schema, const std::string& parent_path = "") {
        bool has_wildcard = schema.contains("*");
        const SchemaNode* wildcard_schema = has_wildcard ? &schema.at("*") : nullptr;
    
        auto recurse = validate_toml_; // Allow reuse in lambdas
    
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
    
            bool valid = std::visit([&](auto&& expected) -> bool {
                using T = std::decay_t<decltype(expected)>;
    
                if constexpr (std::is_same_v<T, TomlType>) {
                    return get_toml_type(node) == expected;
                } else if constexpr (std::is_same_v<T, TomlArray>) {
                    return match_type(node, expected, full_path, recurse);
                } else if constexpr (std::is_same_v<T, std::vector<TomlTypeVariantOneType>>) {
                    for (const auto& variant : expected)
                        if (match_type(node, variant, full_path, recurse))
                            return true;
                    return false;
                }
            }, schema_node.type);
    
            if (!valid) {
                auto type_str = get_toml_type(node) ? to_string(*get_toml_type(node)) : "Unknown";
                return Err(toml::format_error(
                    "Validation failed due to a type mismatch",
                    node,
                    "Type mismatch at '" + full_path + "' (got: " + type_str + ")"
                ));
            }
    
            if (node.is_table() && !schema_node.children.empty()) {
                auto result = validate_toml_(node, schema_node.children, full_path);
                if (!result) return result;
            }
        }

        // Handle wildcard keys
        if (has_wildcard) {
            const auto& tbl = toml_data.as_table();

            for (const auto& [k, node] : tbl) {
                if (schema.contains(k)) continue;
                std::string full_path = parent_path.empty() ? k : parent_path + "." + k;

                bool matched = std::visit([&](auto&& expected) -> bool {
                    using T = std::decay_t<decltype(expected)>;

                    if constexpr (std::is_same_v<T, TomlType>) {
                        return get_toml_type(node) == expected;
                    } else if constexpr (std::is_same_v<T, TomlArray>) {
                        return match_type(node, expected, full_path, recurse);
                    } else if constexpr (std::is_same_v<T, std::vector<TomlTypeVariantOneType>>) {
                        for (const auto& variant : expected)
                            if (match_type(node, variant, full_path, recurse))
                                return true;
                        return false;
                    }
                }, wildcard_schema->type);

                if (!matched) {
                    auto type_str = get_toml_type(node) 
                        ? to_string(*get_toml_type(node)) 
                        : "Unknown";
                    return Err(toml::format_error(
                        "Wildcard type mismatch",
                        node,
                        "Unexpected type at '" + full_path + "' (got: " + type_str + ")"
                    ));
                }

                if (node.is_table() && !wildcard_schema->children.empty()) {
                    auto result = validate_toml_(node, wildcard_schema->children, full_path);
                    if (!result)
                        return result;
                }
            }
        }
        return {}; // Success
    }
    // clang-format on

    Result<void> validate_muuk_toml(const toml::value& toml_data) {
        return validate_toml_(toml_data, muuk_schema);
    };

    Result<void> validate_muuk_lock_toml(const toml::value& toml_data) {
        (void)toml_data;
        return {};
        // return validate_toml_(toml_data, muuk_lock_schema);
    }

} // namespace muuk
