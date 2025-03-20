#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <toml++/toml.h>

#include "rustify.hpp"

namespace muuk {

    enum class TomlType {
        Table,
        Array,
        String,
        Integer,
        Float,
        Boolean,
        Date,
        Time,
        DateTime
    };

    struct SchemaNode;
    typedef std::unordered_map<std::string, SchemaNode> SchemaMap;

    struct TomlArray {
        TomlType element_type;
        std::shared_ptr<SchemaMap> table_schema; // Use shared_ptr instead of unique_ptr

        TomlArray(TomlType type) :
            element_type(type) { }

        TomlArray(const SchemaMap& schema) :
            element_type(TomlType::Table), table_schema(std::make_shared<SchemaMap>(schema)) { }
    };

    using TomlTypeVariantOneType = std::variant<TomlType, TomlArray>;
    using TomlTypeVariant = std::variant<TomlType, TomlArray, std::vector<TomlType>>;

    // Helper to represent multiple possible types
    struct SchemaNode {
        bool required;
        TomlTypeVariant type;
        std::unordered_map<std::string, SchemaNode> children;

        // Constructor to simplify schema definition
        SchemaNode(
            bool required,
            TomlTypeVariant type,
            std::unordered_map<std::string, SchemaNode> children = {}) :
            required(required), type(std::move(type)), children(std::move(children)) { }
    };

    Result<TomlType> get_toml_type(const toml::node& node);

    // Merge multiple SchemaMaps
    template <typename... Maps>
    SchemaMap merge_schema_maps(const Maps&... maps) {
        SchemaMap merged;
        // Fold expression to insert all maps into `merged`
        (merged.insert(maps.begin(), maps.end()), ...);
        return merged;
    }

    const SchemaMap dependency_schema = {
        { "git", { false, TomlType::String } },
        { "muuk_path", { false, TomlType::String } },
        { "version", { true, TomlType::String } }
    };

    const SchemaMap base_package_schema = {
        { "include", { false, TomlArray { TomlType::String } } },
        { "sources", { false, TomlArray { TomlType::String } } },
        { "libs", { false, TomlArray { TomlType::String } } },
        { "cflags", { false, TomlArray { TomlType::String } } },
        { "libflags", { false, TomlArray { TomlType::String } } },
        { "lflags", { false, TomlArray { TomlType::String } } },
        { "system_include", { false, TomlArray { TomlType::String } } }
    };

    // clang-format off
    const SchemaMap muuk_schema = {
        {"package", {true, TomlType::Table, {
            {"name", {true, TomlType::String}},
            {"version", {true, TomlType::String}},
            {"description", {false, TomlType::String}},
            {"license", {false, TomlType::String}},
            {"authors", {false, TomlArray{TomlType::String}}},
            {"repository", {false, TomlType::String}},
            {"documentation", {false, TomlType::String}},
            {"homepage", {false, TomlType::String}},
            {"readme", {false, TomlType::String}},
            {"keywords", {false, TomlArray{TomlType::String}}}
        }}},

        {"dependencies", {false, std::vector<TomlType>{TomlType::String, TomlType::Table}, {
            {"*", {false, TomlType::Table, dependency_schema}}
        }}},

        {"library", {false, TomlType::Table, base_package_schema}},

        {"build", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"include", {false, TomlArray{TomlType::String}}},
                {"cflags", {false, TomlArray{TomlType::String}}},
                {"system_include", {false, TomlArray{TomlType::String}}},
                {"dependencies", {false, TomlType::Table, {}}
            }}}
        }}}},

        {"profile", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"default", {false, TomlType::Boolean}},
                {"inherits", {false, TomlArray{TomlType::String}}},
                {"include", {false, TomlArray{TomlType::String}}},
                {"cflags", {false, TomlArray{TomlType::String}}}
            }}}
        }}},

        {"platform", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"default", {false, TomlType::Boolean}},
                // {"inherits", {false, TomlArray{TomlType::String}}},
                {"include", {false, TomlArray{TomlType::String}}},
                {"cflags", {false, TomlArray{TomlType::String}}},
                {"lflags", {false, TomlArray{TomlType::String}}}

            }}}
        }}},

        {"compiler", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"default", {false, TomlType::Boolean}},
                // {"inherits", {false, TomlArray{TomlType::String}}},
                {"include", {false, TomlArray{TomlType::String}}},
                {"cflags", {false, TomlArray{TomlType::String}}},
                {"lflags", {false, TomlArray{TomlType::String}}}

            }}}
        }}},
    };

    const SchemaMap muuk_lock_schema = {
        {"library", {false, TomlArray(merge_schema_maps(
            SchemaMap{
                {"name", {true, TomlType::String}},
                {"version", {true, TomlType::String}},
                {"dependencies", {false, TomlArray(SchemaMap{
                    {"name", {true, TomlType::String}},
                    {"version", {true, TomlType::String}},
                })}}
            }, 
            base_package_schema
        ))}},
        
        {"build", {false, TomlArray(merge_schema_maps(
            SchemaMap{
                {"name", {true, TomlType::String}},
                {"version", {true, TomlType::String}},
                {"dependencies", {false, TomlArray(SchemaMap{
                    {"name", {true, TomlType::String}},
                    {"version", {true, TomlType::String}},
                })}}
            }, 
            base_package_schema
        ))}},

        {"dependencies", {false, std::vector<TomlType>{TomlType::String, TomlType::Table}, {
            {"*", {false, TomlType::Table, dependency_schema}}
        }}},
    };

    // clang-format on
} // namespace muuk