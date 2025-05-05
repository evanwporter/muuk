// This file contains extensions to the toml11 library for better compatibility with Muuk's needs.
// It includes functions to retrieve values from TOML tables and arrays without throwing.
// Also it uses the `Result` type from Rustify for error handling.

#pragma once
#ifndef MUUK_TOML_EXT_HPP
#define MUUK_TOML_EXT_HPP

#include <toml.hpp>

#include <rustify.hpp>

namespace toml {

    template <typename T>
    std::enable_if_t<
        std::is_same_v<T, int64_t> || std::is_same_v<T, double> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>,
        Result<T>>
    try_get(const value& v) {
        const auto loc = v.location();

        if constexpr (std::is_same_v<T, int64_t>) {
            if (!v.is_integer()) {
                return Err(format_error(
                    "Type error",
                    loc,
                    "Expected a value of type integer, but got " + to_string(v.type()),
                    "hint: change this value to an integer (e.g., 42)"));
            }
            return v.as_integer();
        } else if constexpr (std::is_same_v<T, double>) {
            if (!v.is_floating()) {
                return Err(format_error(
                    "Type error",
                    loc,
                    "Expected a floating-point number, but got " + to_string(v.type()),
                    "hint: add a decimal point, like `3.14`"));
            }
            return v.as_floating();
        } else if constexpr (std::is_same_v<T, bool>) {
            if (!v.is_boolean()) {
                return Err(format_error(
                    "Type error",
                    loc,
                    "Expected a boolean (`true` or `false`), but got " + to_string(v.type())));
            }
            return v.as_boolean();
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!v.is_string()) {
                return Err(format_error(
                    "Type error",
                    loc,
                    "Expected a string, but got " + to_string(v.type()),
                    "hint: wrap the value in double quotes"));
            }
            return v.as_string();
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type in try_get<T>");
        }
    }

    // Counterpart: https://github.com/ToruNiina/toml11/blob/a01fe3b4c14c6d7b99ee3f07c9e80058c6403097/include/toml11/get.hpp#L277C1-L303C2
    template <typename T, typename TC>
    cxx::enable_if_t<
        cxx::conjunction<
            detail::is_container<T>,
            detail::has_push_back_method<T>,
            detail::is_not_toml_type<T, basic_value<TC>>,
            cxx::negation<detail::is_std_basic_string<T>>,
#if defined(TOML11_HAS_STRING_VIEW)
            cxx::negation<detail::is_std_basic_string_view<T>>,
#endif
            cxx::negation<detail::has_from_toml_method<T, TC>>,
            cxx::negation<detail::has_specialized_from<T>>,
            cxx::negation<std::is_constructible<T, const basic_value<TC>&>>>::value,
        Result<T>>
    try_get(const basic_value<TC>& v) {
        using value_type = typename T::value_type;

        if (!v.is_array()) {
            return Err(format_error(
                "Expected array",
                v.location(),
                "This value should be an array to convert into a container"));
        }

        const auto& arr = v.as_array();
        T container;
        detail::try_reserve(container, arr.size());

        for (const auto& elem : arr) {
            Result<value_type> maybe = try_get<value_type>(elem);
            if (!maybe) {
                return Err(maybe);
            }
            container.push_back(std::move(*maybe));
        }

        return std::move(container);
    }

    template <typename T, typename TC>
    cxx::enable_if_t<toml::detail::is_unordered_set<T>::value, T>
    try_get(const toml::basic_value<TC>& v) {
        using Elem = typename detail::is_unordered_set<T>::value_type;

        if (!v.is_array()) {
            return Err(toml::format_error(
                "Expected array",
                v.location(),
                "This value should be an array to convert into a set"));
        }

        const auto& arr = v.as_array();
        T container;

        for (const auto& elem : arr) {
            auto maybe = try_get<Elem>(elem);
            if (!maybe.has_value()) {
                return Err(maybe.error());
            }
            container.insert(std::move(maybe.value()));
        }

        return container;
    }

    // get it to use try_get
    template <typename T, typename TC, typename K>
    T try_find_or(const toml::basic_value<TC>& v, const K& key, T&& fallback) {
        try {
            return toml::get<T>(v.at(toml::detail::key_cast<TC>(key)));
        } catch (...) {
            return std::forward<T>(fallback);
        }
    }

}
#endif // TOML_EXT_HPP