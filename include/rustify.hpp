#pragma once

#ifndef MUUK_RUSTIFY_HPP
#define MUUK_RUSTIFY_HPP

#include <stdexcept>
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
    return tl::unexpected(std::string(msg));
}

inline constexpr auto Err(const char* msg) {
    return tl::unexpected(std::string(msg));
}

template <typename T>
inline constexpr auto Err(Result<T> result) {
    return tl::unexpected(result.error());
}

template <typename... Args>
inline constexpr auto Err(fmt::format_string<Args...> fmt_str, Args&&... args) {
    return tl::unexpected(fmt::format(fmt_str, std::forward<Args>(args)...));
}

struct PanicException : public std::exception {
    std::string message;
    explicit PanicException(std::string msg) :
        message(std::move(msg)) { }
    const char* what() const noexcept override { return message.c_str(); }
};

[[noreturn]] inline void panic(const std::string& msg) {
    throw PanicException(msg);
}

template <typename T>
inline auto Try(Result<T>&& exp) -> Result<T> {
    if (!exp)
        return tl::unexpected(exp.error());
    return std::move(*exp);
}

// Specialization for void type
template <>
inline auto Try<void>(Result<void>&& exp) -> Result<void> {
    if (!exp)
        return tl::unexpected(exp.error());
    return {}; // Return an empty Result<void> (equivalent to Ok())
}

#define Ensure(cond, msg) \
    if (!(cond))          \
    return Err<void>(msg)

// `PanicIfUnreachable(msg)`: Used for logic errors like unreachable!() in Rust.
[[noreturn]] inline void PanicIfUnreachable(const std::string& msg) {
    throw std::runtime_error(fmt::format("Fatal Error: {}", msg));
}

#endif // MUUK_RUSTIFY_HPP