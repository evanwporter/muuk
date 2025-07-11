#pragma once
#ifndef MUUK_LOGGER_H
#define MUUK_LOGGER_H

#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "muukterminal.hpp"
#include "rustify.hpp"

namespace muuk {

    constexpr const char* CRITICAL_PREFIX = "\033[1;41mcritical:\033[0m "; // Red background

    class logger {
    public:
        static void initialize();

        template <typename... Args>
        static void trace(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            spdlog::trace(formatted_message);
        }

        template <typename... Args>
        static void debug(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            spdlog::debug(formatted_message);
        }

        template <typename... Args>
        static void info(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            spdlog::info(formatted_message);
        }

        template <typename... Args>
        static void warn(const std::string& format_str, Args&&... args) {
            const std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            warn(formatted_message);
        }

        template <typename T>
        static inline void warn(const Result<T>& result) {
            warn(result.error().message);
        }

        template <typename... Args>
        static void warn(const std::string& message) {
            initialize();
            muuk::terminal::warn(message);
            spdlog::warn(message);
        }

        static void error(const std::string& msg) {
            initialize();
            muuk::terminal::error(msg);
            std::cerr.flush();
            spdlog::error(msg);
        }

        template <typename... Args>
        static void error(const std::string& format_str, Args&&... args) {
            const std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            error(formatted_message);
        }

        template <typename T>
        static inline void error(const Result<T>& result) {
            error(result.error().message);
        }

        template <typename... Args>
        static void critical(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = fmt::vformat(format_str, fmt::make_format_args(args...));
            std::cerr << CRITICAL_PREFIX << formatted_message << "\n";
            spdlog::critical(formatted_message);
            std::exit(EXIT_FAILURE);
        }

    private:
        static std::once_flag init_flag;
        static std::shared_ptr<spdlog::logger> logger_;
    };

    template <typename ExceptionType = std::runtime_error, typename... Args>
    static void fmt_rt_err(std::string_view fmt, Args&&... args) {
        static_assert(std::is_base_of_v<std::exception, ExceptionType>, "ExceptionType must derive from std::exception");
        throw ExceptionType(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

} // namespace muuk

#endif // MUUK_LOGGER_H
