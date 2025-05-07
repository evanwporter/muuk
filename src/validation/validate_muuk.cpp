#include <string>
#include <unordered_map>
#include <variant>

#include <toml.hpp>

#include "muuk_schema.hpp"
#include "rustify.hpp"

namespace muuk {

    namespace validation {

        inline Result<TomlType> get_toml_type(const toml::value& node) {
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

            return Err("Unknown TOML type", EC::UnknownTOMLType);
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

        using ValidatorFn = std::function<Result<void>(
            const toml::value&,
            const SchemaMap&,
            const std::string&)>;

        Result<void> validate_node_type(const toml::value& node, const TomlTypeVariant& expected, const std::string& path, ValidatorFn recurse);

        Result<void> validate_node_type(
            const toml::value& node,
            const TomlTypeVariantOneType& expected,
            const std::string& path,
            ValidatorFn recurse);

        /// Specialization for TOMLTypeMismatch error code
        template <ErrorCode T = EC::TOMLTypeMismatch>
        inline Result<void> make_error(const toml::value& node, const std::string& path) {
            auto type_str = get_toml_type(node)
                ? to_string(*get_toml_type(node))
                : "Unknown";
            return Err(
                toml::format_error(
                    "Validation failed due to a type mismatch",
                    node,
                    "Type mismatch at '" + path + "' (got: " + type_str + ")"),
                EC::TOMLTypeMismatch);
        }

        /// Validates that a node is a specific primitive TOML type.
        Result<void> validate_primitive(const toml::value& node, TomlType expected, const std::string& path) {
            auto type = get_toml_type(node);
            return (type && *type == expected)
                ? Result<void> {}
                : make_error(node, path);
        }

        /// Validates that a node is a TOML table and recursively validates its fields.
        Result<void> validate_table(const toml::value& node, const TomlTable& schema, const std::string& path, ValidatorFn recurse) {
            if (get_toml_type(node) != TomlType::Table)
                return make_error(node, path);
            return recurse(node, schema.fields, path);
        }

        /// Validates that a node matches at least one of the types in a union.
        Result<void> validate_union(const toml::value& node, const TomlUnionTypes& union_types, const std::string& path, ValidatorFn recurse) {
            for (const auto& expected : union_types) {
                auto result = validate_node_type(node, expected, path, recurse);
                if (result.has_value())
                    return {};
            }
            return make_error(node, path);
        }

        /// Validates that a node is an array and that each element matches the expected schema.
        Result<void> validate_array(
            const toml::value& node,
            const TomlArray& expected,
            const std::string& path,
            ValidatorFn recurse) {

            if (get_toml_type(node) != TomlType::Array)
                return make_error(node, path);

            const auto& arr = node.as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                const auto& item = arr[i];
                std::string item_path = path + "[" + std::to_string(i) + "]";

                // Try to match the element against all possible types
                Result<void> matched = std::visit([&](auto&& subtype) -> Result<void> {
                    using T = std::decay_t<decltype(subtype)>;

                    // Single type
                    if constexpr (std::is_same_v<T, TomlType>) {
                        auto type_result = get_toml_type(item);
                        if (!type_result || *type_result != subtype)
                            return make_error(item, item_path);

                        if (subtype == TomlType::Table && expected.table_schema) {
                            auto result = recurse(item, expected.table_schema->fields, item_path);
                            if (!result)
                                return result;
                        }
                        return {};

                    }

                    // Union of Types
                    else if constexpr (std::is_same_v<T, TomlUnionTypes>) {
                        for (const auto& opt : subtype) {
                            auto result = validate_node_type(item, opt, item_path, recurse);
                            if (result)
                                return {};

                            // Ignore type mismatch errors for union types
                            // since we have to check every type in the union we'll
                            // get a lot of them
                            else if (result.error().code == EC::TOMLTypeMismatch) {
                                continue;
                            }

                            else {
                                return result;
                            }
                        }
                        return make_error(item, item_path);
                    }
                },
                                                  expected.element_types);

                if (!matched)
                    return matched;
            }

            return {};
        }

        /// Dispatches node validation based on its variant type.
        Result<void> validate_node_type(const toml::value& node, const TomlTypeVariant& expected, const std::string& path, ValidatorFn recurse) {
            return std::visit([&](auto&& variant) -> Result<void> {
                using T = std::decay_t<decltype(variant)>;
                if constexpr (std::is_same_v<T, TomlType>) {
                    return validate_primitive(node, variant, path);
                } else if constexpr (std::is_same_v<T, TomlArray>) {
                    return validate_array(node, variant, path, recurse);
                } else if constexpr (std::is_same_v<T, TomlTable>) {
                    return validate_table(node, variant, path, recurse);
                } else if constexpr (std::is_same_v<T, TomlUnionTypes>) {
                    return validate_union(node, variant, path, recurse);
                } else {
                    return make_error(node, path); // fallback
                }
            },
                              expected);
        }

        /// Overload for a single type variant.
        Result<void> validate_node_type(
            const toml::value& node,
            const TomlTypeVariantOneType& expected,
            const std::string& path,
            ValidatorFn recurse) {
            return std::visit([&](const auto& sub) {
                return validate_node_type(node, TomlTypeVariant { sub }, path, recurse);
            },
                              expected);
        }

        /// Top-level function to validate an entire TOML document against a schema.
        Result<void> validate_toml_(const toml::value& data, const SchemaMap& schema, const std::string& parent = "") {
            auto recurse = validate_toml_;
            bool has_wildcard = schema.contains("*");
            const auto* wildcard = has_wildcard ? &schema.at("*") : nullptr;

            for (const auto& [key, node_schema] : schema) {
                if (key == "*")
                    continue;
                std::string path = parent.empty() ? key : parent + "." + key;

                if (!data.contains(key)) {
                    if (node_schema.required) // TODO: Format nicely
                        return Err(
                            toml::format_error(
                                "Missing required key",
                                data,
                                "Required key '" + path + "' was not found in the TOML file."),
                            EC::TOMLRequiredKeyNotFound);
                    continue;
                }

                const auto& node = data.at(key);
                auto result = validate_node_type(node, node_schema.type, path, recurse);
                if (!result)
                    return result;
            }

            if (has_wildcard) {
                for (const auto& [k, node] : data.as_table()) {
                    if (schema.contains(k))
                        continue;
                    std::string path = parent.empty() ? k : parent + "." + k;

                    auto result = validate_node_type(node, wildcard->type, path, recurse);
                    if (!result)
                        return result;
                }
            }

            return {};
        }

        Result<void> validate_muuk_toml(const toml::value& toml_data) {
            auto result = validation::validate_toml_(toml_data, muuk_schema);
            return result.has_value()
                ? Result<void> {}
                : Err(result);
        }

        // TODO: add a schema for muuk_lock.toml
        Result<void> validate_muuk_lock_toml(const toml::value& toml_data) {
            auto result = validation::validate_toml_(toml_data, muuk_lock_schema);
            return result.has_value()
                ? Result<void> {}
                : Err(result);
        }

    } // namespace validation
} // namespace muuk
