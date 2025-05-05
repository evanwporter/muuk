#pragma once

#include <array>
#include <filesystem>

#include <fmt/core.h>

#include "rustify.hpp"

namespace muuk {
    enum class ErrorCode {
        FileNotFound,
        MuukTOMLNotFound,
        UnknownError,
        Count // Always keep this last
    };

    using EC = ErrorCode;

    // Order must match ErrorCode enum
    constexpr std::array<const char*, static_cast<size_t>(ErrorCode::Count)> error_templates = {
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
}