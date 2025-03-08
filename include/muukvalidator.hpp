// Contains various utilities to validate different parts of the muuk.toml and muuk.lock.toml files.
// Taken heavily from cabinpkg @ github.com/cabinpkg/cabin w/ modifications
#pragma once
#ifndef MUUKVALIDATOR_HPP
#define MUUKVALIDATOR_HPP

#include "compiler.hpp"
#include "logger.h"
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

    class invalid_toml : public std::runtime_error {
    public:
        explicit invalid_toml(const std::string& message)
            : std::runtime_error(message) {}
    };

    enum class TomlType { Table, Array, String, Integer, Float, Boolean, Date, Time, DateTime };

    struct SchemaNode {
        bool required;
        TomlType type;
        std::optional<std::vector<TomlType>> allowed_types = std::nullopt;
        std::optional<std::unordered_map<std::string, SchemaNode>> children = std::nullopt;
    };

    typedef std::unordered_map<std::string, SchemaNode> SchemaMap;

    Result<TomlType> get_toml_type(const toml::node& node);

    Result<void> validate_toml_(
        const toml::table& toml_data,
        const SchemaMap& schema,
        std::string parent_path = ""
    );

    Result<void> validate_muuk_toml(const toml::table& toml_data);
    Result<void> validate_muuk_lock_toml(const toml::table& toml_data);

    bool is_valid_dependency_name(std::string_view name);

    Result<bool> validate_flag(Compiler compiler_, std::string_view flag);

} // namespace muuk

#endif // MUUKVALIDATOR_HPP