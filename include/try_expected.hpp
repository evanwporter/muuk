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

#include <tl/expected.hpp>

// TRYV: use when expr returns std::expected<void, E>
#define TRYV(expr)                                      \
    do {                                                \
        auto _try_result = (expr);                      \
        if (!_try_result)                               \
            return tl::unexpected(_try_result.error()); \
    } while (0)
