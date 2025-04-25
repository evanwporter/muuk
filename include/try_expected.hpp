// try_expected.h
// Helper macros for working with std::expected
// Inspired by P0779R0 and Rust's `?` operator

// Example use:
// std::expected<int, std::string> foo();
// std::expected<void, std::string> bar();
//
// int x = TRY(foo());
// TRYV(bar());
// int y = TRY_OR(foo(), [](auto e) { return tl::unexpected("Wrapped: " + e); });

#pragma once

// Detect whether we are compiling with GCC/Clang for statement expressions
#if defined(__GNUC__) || defined(__clang__)
#define TRY_HAS_STATEMENT_EXPR 1
#else
#define TRY_HAS_STATEMENT_EXPR 0
#endif

#include <tl/expected.hpp>

#if TRY_HAS_STATEMENT_EXPR
#else
#include <type_traits>
#include <utility>
#endif

// Basic TRY macro using statement expressions (non-portable)
#if TRY_HAS_STATEMENT_EXPR
#define TRY(expr)                                       \
    ({                                                  \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
        std::move(*_try_result);                        \
    })
#else
// Portable version using lambda
#define TRY(expr)                                       \
    ([&]() -> decltype(auto) {                          \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
        return std::move(*_try_result);                 \
    }())
#endif

// TRYV: use when expr returns std::expected<void, E>
#define TRYV(expr)                                      \
    do {                                                \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
    } while (0)

// TRY_OR: custom error handling function/lambda
#define TRY_OR(expr, handler)                      \
    ([&]() -> decltype(auto) {                     \
        auto _try_result = (expr);                 \
        if (!_try_result)                          \
            return (handler)(_try_result.error()); \
        return std::move(*_try_result);            \
    }())

// TRYV_OR: custom error handler for void-returning std::expected
#define TRYV_OR(expr, handler)                     \
    do {                                           \
        auto _try_result = (expr);                 \
        if (!_try_result)                          \
            return (handler)(_try_result.error()); \
    } while (0)
