#pragma once
#ifndef MUUK_RUSTIFY_HPP
#define MUUK_RUSTIFY_HPP

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>
#include <tl/expected.hpp>

enum class ErrorCode {
    FileNotFound,
    MuukTOMLNotFound,
    TOMLTypeMismatch,
    TOMLRequiredKeyNotFound,
    UnknownTOMLType,
    Unknown,
    Count // Always keep this last
};

using EC = ErrorCode;

struct Error {
    std::string message;
    ErrorCode code = ErrorCode::Unknown;

    Error(std::string msg, ErrorCode c = ErrorCode::Unknown) :
        message(msg), code(c) { }

    // Implicit conversion from a string
    Error(const char* msg) :
        message(msg), code(ErrorCode::Unknown) { }

    /// Formatting-style constructor
    template <typename... Args>
    static Error formatted(ErrorCode code, fmt::format_string<Args...> fmt_str, Args&&... args) {
        return Error(fmt::format(fmt_str, std::forward<Args>(args)...), code);
    }
};

template <>
struct fmt::formatter<Error> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Error& err, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[{}] {}", static_cast<int>(err.code), err.message);
    }
};

template <typename T, typename E = Error>
using Result = tl::expected<T, E>;

inline constexpr auto Err(const std::string& msg) {
    return tl::unexpected(msg);
}

template <typename T>
inline constexpr auto Err(T&& value) {
    return tl::unexpected(std::forward<T>(value));
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

inline auto Err(std::string msg, ErrorCode code) {
    return Err(Error { std::move(msg), code });
}

template <typename... Args>
inline auto Err(ErrorCode code, fmt::format_string<Args...> fmt_str, Args&&... args) {
    return tl::unexpected(Error::formatted(code, fmt_str, std::forward<Args>(args)...));
}

// ======================================================

// Helper macros for working with std::expected
// Inspired by P0779R0 and Rust's `?` operator

// TRYV: use when expr returns std::expected<void, E>
#define TRYV(expr)                                      \
    do {                                                \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
    } while (0)

// ======================================================

// Order must match ErrorCode enum
constexpr static std::array<const char*, static_cast<size_t>(ErrorCode::Count)> error_templates = {
    "File '{}' does not exist", // FileNotFound
    "Could not find `muuk.toml` in `{}`", // MuukTOMLNotFound
    "An unknown error occurred: {}" // UnknownError
};

/// Returns a formatted error message from the code
template <ErrorCode code, typename... Args>
constexpr inline auto make_error(Args&&... args) {
    constexpr size_t index = static_cast<size_t>(code);
    static_assert(index < error_templates.size(), "ErrorCode index out of bounds");

    return Err(fmt::format(error_templates[index], std::forward<Args>(args)...));
}

/// Specialization for MuukTOMLNotFound error code
template <>
inline auto make_error<EC::MuukTOMLNotFound>(const std::string& path) {
    std::string dir = std::filesystem::absolute(path).parent_path().string();
    return make_error<EC::MuukTOMLNotFound>(dir); // Not recusive since `path` is const
}
#endif // MUUK_RUSTIFY_HPP