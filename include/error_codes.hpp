#pragma once

#include <array>

#include <fmt/core.h>

#include "rustify.hpp"

namespace muuk {
    enum class ErrorCode {
        FileNotFound,
        MuukTOMLMissingSection,
        UnknownError,
        Count // Always keep this last
    };

    using EC = ErrorCode;

    // Order must match ErrorCode enum
    constexpr std::array<const char*, static_cast<size_t>(ErrorCode::Count)> error_templates = {
        "File '{}' does not exist.", // FileNotFound
        "An unknown error occurred: {}" // UnknownError
    };

    // Returns a formatted error message from the code
    template <ErrorCode code, typename... Args>
    constexpr auto make_error(Args&&... args) {
        constexpr size_t index = static_cast<size_t>(code);
        static_assert(index < error_templates.size(), "ErrorCode index out of bounds");

        return Err(fmt::format(error_templates[index], std::forward<Args>(args)...));
    }
}
