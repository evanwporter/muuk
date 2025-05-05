#pragma once

#ifndef MUUK_RUSTIFY_HPP
#define MUUK_RUSTIFY_HPP

#include <string>
#include <string_view>
#include <utility>

#include <fmt/core.h>
#include <tl/expected.hpp>

template <typename T, typename E = std::string>
using Result = tl::expected<T, E>;

template <typename T>
constexpr Result<T> Ok(T value) {
    return std::move(value);
}

inline constexpr auto Err(const std::string& msg) {
    return tl::unexpected(msg);
}

inline constexpr auto Err(std::string_view msg) {
    return Err(std::string(msg));
}

inline constexpr auto Err(const char* msg) {
    return Err(std::string(msg));
}

template <typename T>
inline constexpr auto Err(Result<T> result) {
    return Err(result.error());
}

template <typename... Args>
inline constexpr auto Err(fmt::format_string<Args...> fmt_str, Args&&... args) {
    return Err(fmt::format(fmt_str, std::forward<Args>(args)...));
}

// Helper macros for working with std::expected
// Inspired by P0779R0 and Rust's `?` operator

// TRYV: use when expr returns std::expected<void, E>
#define TRYV(expr)                                      \
    do {                                                \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
    } while (0)

#endif // MUUK_RUSTIFY_HPP