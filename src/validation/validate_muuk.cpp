#include <string>
#include <unordered_map>
#include <variant>

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
    bool match_type(
        const toml::value& node,
        const TomlTypeVariantOneType& expected_type,
        const std::string& path,
        std::function<Result<void>(const toml::value&, const SchemaMap&, const std::string&)> recurse) {
        auto node_type = get_toml_type(node);
        if (!node_type)
            return false;

        return std::visit([&](auto&& expected) -> bool {
            using T = std::decay_t<decltype(expected)>;

            // Case 1: Expected a primitive TOML type
            if constexpr (std::is_same_v<T, TomlType>) {
                return *node_type == expected;
            }

            // Case 2: Expected an array
            else if constexpr (std::is_same_v<T, TomlArray>) {
                if (*node_type != TomlType::Array)
                    return false;

                const auto& arr = node.as_array();

                for (const auto& item : arr) {
                    auto item_type = get_toml_type(item);
                    if (!item_type)
                        return false;

                    // Inner array match
                    // Resolve whether this item matches any of the allowed element types
                    bool matched = std::visit([&](auto&& array_expected) -> bool {
                        using A = std::decay_t<decltype(array_expected)>;

                        // Case 2a: single element type (homogenous array)
                        if constexpr (std::is_same_v<A, TomlType>) {
                            if (*item_type != array_expected)
                                return false;

                            // Recursively validate array of tables
                            if (array_expected == TomlType::Table && expected.table_schema) {
                                auto result = recurse(item, expected.table_schema->fields, path + "[]");
                                return result.has_value();
                            }

                            return true;
                        }

                        // Case 2b: union of element types (heterogenous array)
                        else if constexpr (std::is_same_v<A, TomlUnionTypes>) {
                            for (const auto& variant : array_expected) {
                                if (match_type(item, variant, path + "[]", recurse))
                                    return true;
                            }
                            return false;
                        }
                    }, expected.element_types);

                    if (!matched)
                        return false;
                }

                return true;
            }

            // Case 3: Expected one of multiple types (not an array)
            else if constexpr (std::is_same_v<T, TomlUnionTypes>) {
                for (const auto& variant : expected) {
                    if (match_type(node, variant, path, recurse))
                        return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, TomlTable>) {
                if (*node_type != TomlType::Table)
                    return false;
                return recurse(node, expected.fields, path).has_value();
            }
    
        }, expected_type);
    }

    bool validate_node_type(
        const toml::value& node,
        const TomlTypeVariant& expected,
        const std::string& path,
        const std::function<Result<void>(const toml::value&, const SchemaMap&, const std::string&)>& recurse
    ) {
        return std::visit([&](auto&& variant) -> bool {
            using T = std::decay_t<decltype(variant)>;
    
            if constexpr (std::is_same_v<T, TomlType>) {
                auto t = get_toml_type(node);
                return t && *t == variant;
    
            } else if constexpr (std::is_same_v<T, TomlArray>) {
                return match_type(node, variant, path, recurse);
    
            } else if constexpr (std::is_same_v<T, TomlTable>) {
                auto t = get_toml_type(node);
                if (!t || *t != TomlType::Table)
                    return false;
                return recurse(node, variant.fields, path).has_value();
    
            } else if constexpr (std::is_same_v<T, TomlUnionTypes>) {
                for (const auto& alt : variant)
                    if (match_type(node, alt, path, recurse))
                        return true;
                return false;
            }
        }, expected);
    }
    // clang-format on

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

            bool valid = validate_node_type(
                node,
                schema_node.type,
                full_path,
                recurse);

            if (!valid) {
                auto type_str = get_toml_type(node) ? to_string(*get_toml_type(node)) : "Unknown";
                return Err(toml::format_error(
                    "Validation failed due to a type mismatch",
                    node,
                    "Type mismatch at '" + full_path + "' (got: " + type_str + ")"));
            }
        }

        // Handle wildcard keys
        if (has_wildcard) {
            const auto& tbl = toml_data.as_table();

            for (const auto& [k, node] : tbl) {
                if (schema.contains(k))
                    continue;
                std::string full_path = parent_path.empty() ? k : parent_path + "." + k;

                bool matched = validate_node_type(
                    node,
                    wildcard_schema->type,
                    full_path,
                    recurse);

                if (!matched) {
                    auto type_str = get_toml_type(node)
                        ? to_string(*get_toml_type(node))
                        : "Unknown";

                    return Err(toml::format_error(
                        "Wildcard type mismatch",
                        node,
                        "Unexpected type at '" + full_path + "' (got: " + type_str + ")"));
                }
            }
        }
        return {}; // Success
    }

    Result<void> validate_muuk_toml(const toml::value& toml_data) {
        return validate_toml_(toml_data, muuk_schema);
    };

    // TODO: add a schema for muuk_lock.toml
    Result<void> validate_muuk_lock_toml(const toml::value& toml_data) {
        (void)toml_data;
        return {};
        // return validate_toml_(toml_data, muuk_lock_schema);
    }

} // namespace muuk
