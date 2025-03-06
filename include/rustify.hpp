#pragma once

#ifndef MUUK_RUSTIFY_HPP
#define MUUK_RUSTIFY_HPP

#include <tl/expected.hpp>
#include <fmt/core.h>
#include <optional>
#include <string>

enum class ErrorType {
    RuntimeError,
    LogicError,
    IOError,
};

struct Error {
    ErrorType type;
    std::string message;
};

template <typename T, typename E = std::string>
using Result = tl::expected<T, E>;
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

template <typename... Args>
inline constexpr auto Err(fmt::format_string<Args...> fmt_str, Args&&... args) {
    return tl::unexpected(fmt::format(fmt_str, std::forward<Args>(args)...));
}

// #define Try(expr) ({ auto&& res = (expr); if (!res) return Err(res.error()); std::move(res.value()); })
template <typename T>
constexpr decltype(auto) TRY(Result<T>&& exp) {
    if (!exp) return tl::unexpected(exp.error());
    return std::move(*exp);
}

#define Ensure(cond, msg) if (!(cond)) return Err<void>(msg)

// `PanicIfUnreachable(msg)`: Used for logic errors like unreachable!() in Rust.
[[noreturn]] inline void PanicIfUnreachable(const std::string& msg) {
    throw std::runtime_error(fmt::format("Fatal Error: {}", msg));
}

// Rust-Like Option<T> (Safer than std::optional<T>)
template <typename T>
class Option {
private:
    std::optional<T> value_;

public:
    Option() = default; // Default: None
    explicit Option(const T& value) : value_(value) {}
    explicit Option(T&& value) : value_(std::move(value)) {}

    // Equivalent to Rust's `Some(value)`
    static Option Some(T value) { return Option(std::move(value)); }

    // Equivalent to Rust's `None`
    static Option None() { return Option(); }

    // is_some() -> true if it contains a value
    bool is_some() const { return value_.has_value(); }

    // is_none() -> true if empty
    bool is_none() const { return !value_.has_value(); }

    // unwrap() -> Panics if empty
    T& unwrap() {
        if (!value_) {
            PanicIfUnreachable("Called unwrap() on None");
        }
        return *value_;
    }

    // expect(msg) -> Panics if empty, but with a message
    T& expect(const std::string& msg) {
        if (!value_) {
            PanicIfUnreachable(msg);
        }
        return *value_;
    }

    // unwrap_or(default) -> Returns default value if empty
    T unwrap_or(const T& default_value) const {
        return value_.value_or(default_value);
    }

    // unwrap_or_else(func) -> Calls func() to get default value if empty
    template <typename F>
    T unwrap_or_else(F func) const {
        return value_ ? *value_ : func();
    }

    // map(func) -> Transforms value if present
    template <typename F, typename U = std::invoke_result_t<F, T&>>
    Option<U> map(F func) const {
        if (value_) {
            return Option<U>(func(*value_));
        }
        return Option<U>::None();
    }
};

#endif // MUUK_RUSTIFY_HPP