// Contains various utilities to validate different parts of the muuk.toml and muuk.lock.toml files.
// Taken heavily from cabinpkg @ github.com/cabinpkg/cabin w/ modifications
#pragma once
#ifndef MUUKVALIDATOR_HPP
#define MUUKVALIDATOR_HPP

#include <stdexcept>
#include <string>

#include <toml++/toml.h>

#include "compiler.hpp"
#include "rustify.hpp"

namespace muuk {

    class invalid_toml : public std::runtime_error {
    public:
        explicit invalid_toml(const std::string& message) :
            std::runtime_error(message) { }
    };

    Result<void> validate_muuk_toml(const toml::table& toml_data);
    Result<void> validate_muuk_lock_toml(const toml::table& toml_data);

    bool is_valid_dependency_name(std::string_view name);

    Result<bool> validate_flag(Compiler compiler_, std::string_view flag);

} // namespace muuk

#endif // MUUKVALIDATOR_HPP