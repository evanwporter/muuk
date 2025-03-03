#pragma once

#ifndef MUUK_RUSTIFY_HPP
#define MUUK_RUSTIFY_HPP

#include <tl/expected.hpp>
#include <fmt/core.h>

template <typename T>
using Result = tl::expected<T, std::string>;

template <typename T>
constexpr Result<T> Ok(T value) {
    return std::move(value);
}

inline constexpr auto Err(const std::string& msg) {
    return tl::unexpected(msg);
}

inline constexpr auto Err(std::string_view msg) {
    return tl::unexpected(std::string(msg));
}

inline constexpr auto Err(const char* msg) {
    return tl::unexpected(std::string(msg));
}

template <typename... Args>
inline constexpr auto Err(fmt::format_string<Args...> fmt_str, Args&&... args) {
    return tl::unexpected(fmt::format(fmt_str, std::forward<Args>(args)...));
}

#define Try(expr) ({ auto&& res = (expr); if (!res) return Err(res.error()); std::move(res.value()); })
#define Ensure(cond, msg) if (!(cond)) return Err<void>(msg)

#endif // MUUK_RUSTIFY_HPP