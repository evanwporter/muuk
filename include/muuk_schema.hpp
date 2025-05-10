#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <toml.hpp>

namespace muuk {

    namespace validation {

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

        struct TomlArray;
        struct TomlTable;

        /// Helpers to represent multiple possible types
        using TomlTypeVariantOneType = std::variant<TomlType, TomlArray, TomlTable>;
        using TomlUnionTypes = std::vector<TomlTypeVariantOneType>;

        /// This is for the ScemaNode type, which can be a single type or a union of types.
        using TomlTypeVariant = std::variant<TomlType, TomlArray, TomlTable, TomlUnionTypes>;

        struct SchemaNode;
        typedef std::unordered_map<std::string, SchemaNode> SchemaMap;

        struct TomlTable {
            SchemaMap fields;

            explicit TomlTable(SchemaMap fields_) :
                fields(std::move(fields_)) { }
        };

        struct TomlArray {
            std::variant<TomlType, TomlUnionTypes> element_types;
            std::shared_ptr<TomlTable> table_schema;

            TomlArray(TomlType type) :
                element_types(type) { }

            TomlArray(const TomlUnionTypes& types) :
                element_types(types) { }

            TomlArray(const SchemaMap& schema) :
                element_types(TomlType::Table),
                table_schema(std::make_shared<TomlTable>(schema)) { }
        };

        // Helper to represent multiple possible types
        struct SchemaNode {
            bool required;
            TomlTypeVariant type;

            SchemaNode(bool required_, TomlTypeVariant type_) :
                required(required_), type(std::move(type_)) { }
        };

        // Merge multiple SchemaMaps
        template <typename... Maps>
        SchemaMap merge_schema_maps(const Maps&... maps) {
            SchemaMap merged;
            // Fold expression to insert all maps into `merged`
            (merged.insert(maps.begin(), maps.end()), ...);
            return merged;
        }

        // clang-format off

        const SchemaMap dependency_schema = {
            { "version", { true, TomlType::String } },
            { "git", { false, TomlType::String } },
            { "path", { false, TomlType::String } },
            { "features", { false, TomlArray { TomlType::String } } },
            { "system", { false, TomlType::Boolean } },
            { "libs", { false, TomlArray { TomlType::String } } }
        };

        const SchemaMap base_package_schema = {
            { "include", { false, TomlArray { TomlType::String } } },
            { "sources", { false, TomlArray {
                TomlUnionTypes {
                    TomlType::String,
                    TomlTable({
                        {"path", { true, TomlType::String } },
                        {"cflags", { false, TomlArray { TomlType::String } } },
                    })
                }}}},
            { "libs", { false, TomlArray {
                TomlUnionTypes {
                    TomlType::String,
                    TomlTable({
                        {"path", { true, TomlType::String } },
                        {"lflags", { false, TomlArray { TomlType::String } } },
                        {"compiler", { false, TomlType::String } },
                        {"platform", { false, TomlType::String } },
                    })
                }}}},
            { "cflags", { false, TomlArray { TomlType::String } } },
            { "libflags", { false, TomlArray { TomlType::String } } },
            { "lflags", { false, TomlArray { TomlType::String } } },
            { "system_include", { false, TomlArray { TomlType::String } } }
        };

        const SchemaMap build_schema = {
            { "*", { false, TomlTable({
                { "include", { false, TomlArray{TomlType::String} } },
                { "cflags", { false, TomlArray{TomlType::String} } },
                { "system_include", { false, TomlArray{TomlType::String} } },
                { "link", { false, TomlType::String } },
                { "dependencies", { false, TomlTable({}) } }
            })}}
        }; 

        const SchemaMap package_schema = {
            {"name", {true, TomlType::String}},
            {"version", {true, TomlType::String}},
            {"cxx_standard", {false, TomlType::String}},
            {"c_standard", {false, TomlType::String}},
            {"git", {false, TomlType::String}},
            {"description", {false, TomlType::String}},
            {"license", {false, TomlType::String}},
            {"authors", {false, TomlArray{TomlType::String}}},
            {"repository", {false, TomlType::String}},
            {"documentation", {false, TomlType::String}},
            {"homepage", {false, TomlType::String}},
            {"readme", {false, TomlType::String}},
            {"keywords", {false, TomlArray{TomlType::String}}}
        };

        const SchemaMap muuk_schema = {
            {"package", {true, TomlTable(package_schema)}},

            { "dependencies", { false, TomlUnionTypes{
                TomlType::String,
                TomlTable({
                    { "*", { false, TomlUnionTypes{
                        TomlType::String,
                        TomlTable({dependency_schema})
                    }}}
                })
            }}},

            {"build", {false, TomlTable(build_schema)}},
            {"library", {false, TomlTable {base_package_schema}}},

            {"profile", {false, TomlTable({
                {"*", {false, TomlTable({
                    {"default", {false, TomlType::Boolean}},
                    {"inherits", {false, TomlArray{TomlType::String}}},
                    {"include", {false, TomlArray{TomlType::String}}},
                    {"cflags", {false, TomlArray{TomlType::String}}}
                })}}
            })}},
        
            {"platform", {false, TomlTable({
                {"*", {false, TomlTable({
                    {"default", {false, TomlType::Boolean}},
                    {"include", {false, TomlArray{TomlType::String}}},
                    {"cflags", {false, TomlArray{TomlType::String}}},
                    {"lflags", {false, TomlArray{TomlType::String}}}
                })}}
            })}},
        
            {"compiler", {false, TomlTable({
                {"*", {false, TomlTable({
                    {"default", {false, TomlType::Boolean}},
                    {"include", {false, TomlArray{TomlType::String}}},
                    {"cflags", {false, TomlArray{TomlType::String}}},
                    {"lflags", {false, TomlArray{TomlType::String}}}
                })}}
            })}},
        
            {"features", {false, TomlTable({
                {"default", {false, TomlArray{TomlType::String}}},
                {"*", {false, TomlUnionTypes{
                    TomlTypeVariantOneType{TomlArray{TomlType::String}},
                    TomlTypeVariantOneType{TomlTable({
                        {"dependencies", {false, TomlArray{TomlType::String}}},
                        {"defines", {false, TomlArray{TomlType::String}}}
                    })}
                }}}
            })}}
        };

        const SchemaMap muuk_lock_schema = {
            // {"library", {false, TomlArray(merge_schema_maps(
            //     SchemaMap{
            //         {"name", {true, TomlType::String}},
            //         {"version", {true, TomlType::String}},
            //         {"dependencies", {false, TomlArray(SchemaMap{
            //             {"name", {true, TomlType::String}},
            //             {"version", {true, TomlType::String}},
            //         })}}
            //     }, 
            //     base_package_schema
            // ))}},
            
            // {"build", {false, TomlArray(merge_schema_maps(
            //     SchemaMap{
            //         {"name", {true, TomlType::String}},
            //         {"version", {true, TomlType::String}},
            //         {"dependencies", {false, TomlArray(SchemaMap{
            //             {"name", {true, TomlType::String}},
            //             {"version", {true, TomlType::String}},
            //         })}}
            //     }, 
            //     base_package_schema
            // ))}},

            // {"dependencies", {false, TomlUnionTypes{TomlType::String, TomlType::Table}, {
            //     {"*", {false, TomlType::Table, dependency_schema}}
            // }}},
        };

        // clang-format on
    } // namespace validation
} // namespace muuk