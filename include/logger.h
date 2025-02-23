#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <chrono>
#include <format>
#include <iostream>
// #include <cstdlib>

namespace muuk {

    constexpr const char* WARN_PREFIX = "\033[1;33mwarning:\033[0m "; // Yellow
    constexpr const char* ERROR_PREFIX = "\033[1;31merror:\033[0m "; // Red
    constexpr const char* CRITICAL_PREFIX = "\033[1;41mcritical:\033[0m "; // Red background

    class logger {
    public:
        static void initialize();

        template<typename... Args>
        static void trace(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            spdlog::trace(formatted_message);
        }

        template<typename... Args>
        static void debug(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            spdlog::debug(formatted_message);
        }

        template<typename... Args>
        static void info(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            spdlog::info(formatted_message);
        }

        template<typename... Args>
        static void warn(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            std::cerr << WARN_PREFIX << formatted_message << "\n";
            spdlog::warn(formatted_message);
        }

        template<typename... Args>
        static void error(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            spdlog::error(formatted_message);
            throw std::runtime_error(ERROR_PREFIX + formatted_message);
        }

        template<typename... Args>
        static void critical(const std::string& format_str, Args&&... args) {
            initialize();
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            std::cerr << CRITICAL_PREFIX << formatted_message << "\n";
            spdlog::critical(formatted_message);
            std::exit(EXIT_FAILURE);
        }

    private:
        static std::once_flag init_flag;
        static std::shared_ptr<spdlog::logger> logger_;
    };

} // namespace muuk