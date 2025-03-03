#pragma once
#include <tl/expected.hpp>
#include <string>
#include <fmt/core.h>
#include <format>

template <typename T>
using Result = tl::expected<T, std::string>;

template <typename T>
constexpr Result<T> Ok(T value) {
    return std::move(value);
}

template <typename... Args>
constexpr auto Err(fmt::format_string<Args...> fmt_str, Args&&... args) {
    return tl::unexpected(fmt::format(fmt_str, std::forward<Args>(args)...));
}

#define Try(expr) ({ auto&& res = (expr); if (!res) return Err(res.error()); std::move(res.value()); })
#define Ensure(cond, msg) if (!(cond)) return Err<void>(msg)

// template <typename... Args>
// constexpr auto Bail(Args&&... args) {
//     return Err<void>(std::format(std::forward<Args>(args)...));
// }

namespace muuk {
    namespace compiler {
        enum class Compiler {
            GCC,
            Clang,
            MSVC
        };

        std::string to_string(Compiler compiler);
        Compiler from_string(const std::string& compiler_str);
    }

    void init_project();
    tl::expected<void, std::string> qinit_library(
        const std::string& author,
        const std::string& repo
    );

    // flag handler
    std::string normalize_flag(const std::string& flag, const compiler::Compiler compiler);
    std::string normalize_flags(const std::vector<std::string>& flags, const compiler::Compiler compiler);
    void normalize_flags_inplace(std::vector<std::string>& flags, const compiler::Compiler compiler);
}