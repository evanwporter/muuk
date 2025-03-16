#pragma once

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

    struct TomlArray {
        TomlType element_type;
    };

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

    typedef std::unordered_map<std::string, SchemaNode> SchemaMap;

    Result<TomlType> get_toml_type(const toml::node& node);

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

        {"dependencies", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"git", {false, TomlType::String}},
                {"muuk_path", {false, TomlType::String}},
                {"version", {true, TomlType::String}}
            }}}
        }}},

        {"library", {false, TomlType::Table, {
            {"include", {false, TomlArray{TomlType::String}}},
            {"source", {false, TomlArray{TomlType::String}}},
            {"cflags", {false, TomlArray{TomlType::String}}},
            {"libflags", {false, TomlArray{TomlType::String}}},
            {"lflags", {false, TomlArray{TomlType::String}}},
            {"system_include", {false, TomlArray{TomlType::String}}}
        }}},

        {"build", {false, TomlType::Table, {
            {"*", {false, TomlType::Table, {
                {"include", {false, TomlArray{TomlType::String}}},
                {"cflags", {false, TomlArray{TomlType::String}}},
                {"system_include", {false, TomlArray{TomlType::String}}},
                {"dependencies", {false, TomlType::Table, {}}
            }}}
        }}}},

        {"profile", {false, TomlType::Table, {
            {"default", {false, TomlType::Boolean}},
            {"inherits", {false, TomlType::String}},
            {"cflags", {false, TomlArray{TomlType::String}}},
            {"lflags", {false, TomlArray{TomlType::String}}}
        }}},

        {"platform", {false, TomlType::Table, {
            {"cflags", {false, TomlArray{TomlType::String}}},
            {"lflags", {false, TomlArray{TomlType::String}}}
        }}},

        {"compiler", {false, TomlType::Table, {
            {"cflags", {false, TomlArray{TomlType::String}}},
            {"lflags", {false, TomlArray{TomlType::String}}}
        }}}
    };

    // clang-format on
} // namespace muuk