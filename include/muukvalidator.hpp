// Contains various utilities to validate different parts of the muuk.toml and muuk.lock.toml files.
// Taken heavily from cabinpkg @ github.com/cabinpkg/cabin w/ modifications
#pragma once
#ifndef MUUKVALIDATOR_HPP
#define MUUKVALIDATOR_HPP

#include <string_view>

#include <toml.hpp>

#include "compiler.hpp"
#include "rustify.hpp"

namespace muuk {

    Result<void> validate_muuk_toml(const toml::value& toml_data);
    Result<void> validate_muuk_lock_toml(const toml::value& toml_data);

    bool is_valid_dependency_name(std::string_view name);

    Result<bool> validate_flag(Compiler compiler_, std::string_view flag);

} // namespace muuk

#endif // MUUKVALIDATOR_HPP