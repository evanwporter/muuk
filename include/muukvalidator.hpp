#pragma once
#ifndef MUUKVALIDATOR_HPP
#define MUUKVALIDATOR_HPP

#include "logger.h"

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

    TomlType get_toml_type(const toml::node& node);

    bool validate_toml_(
        const toml::table& toml_data,
        const std::unordered_map<std::string, SchemaNode>& schema,
        std::string parent_path = ""
    );

    bool validate_muuk_toml(const toml::table& toml_data);
} // namespace muuk

#endif // MUUKVALIDATOR_HPP