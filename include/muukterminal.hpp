#pragma once
#ifndef MUUK_TERMINAL_HPP
#define MUUK_TERMINAL_HPP

#include <cstdio>
#include <string>
#include <vector>

#include <fmt/color.h>
#include <fmt/core.h>

#define KEY_UP 72 // Up arrow key
#define KEY_DOWN 80 // Down arrow key
#define KEY_ENTER '\r' // Enter key

namespace muuk {
    namespace terminal {

        // Styling
        struct style {
            static inline const std::string RESET = "\033[0m";
            static inline const std::string BOLD = "\033[1m";
            static inline const std::string DIM = "\033[2m";

            static inline const std::string RED = "\033[31m";
            static inline const std::string GREEN = "\033[32m";
            static inline const std::string YELLOW = "\033[33m";
            static inline const std::string BLUE = "\033[34m";
            static inline const std::string MAGENTA = "\033[35m";
            static inline const std::string CYAN = "\033[36m";
            static inline const std::string WHITE = "\033[37m";
        };

        inline constexpr fmt::color color_from_name(std::string_view name) {
            if (name == "red")
                return fmt::color::red;
            if (name == "green")
                return fmt::color::green;
            if (name == "yellow")
                return fmt::color::yellow;
            if (name == "blue")
                return fmt::color::blue;
            if (name == "magenta")
                return fmt::color::magenta;
            if (name == "cyan")
                return fmt::color::cyan;
            if (name == "white")
                return fmt::color::white;
            if (name == "black")
                return fmt::color::black;

            return fmt::color::white; // Default fallback
        }

        template <typename... Args>
        inline void info(std::string_view color_name, fmt::format_string<Args...> format_str, Args&&... args) {
            fmt::print(
                fg(color_from_name(color_name)),
                format_str,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        void info(fmt::format_string<Args...> format_str, Args&&... args) {
            std::string message = fmt::format(format_str, std::forward<Args>(args)...);
            fmt::print(
                fg(fmt::color::cyan),
                "{}\n",
                message);
        }

        template <typename... Args>
        void warn(const std::string& format_str, Args&&... args) {
            std::string message = fmt::vformat(format_str, fmt::make_format_args(args...));
            fmt::print(
                stderr,
                fg(fmt::color::yellow),
                "warning:{} {}\n",
                style::RESET,
                message);
        }

        template <typename... Args>
        void error(const std::string& format_str, Args&&... args) {
            const std::string message = fmt::vformat(format_str, fmt::make_format_args(args...));
            error(message);
        }

        inline void error(const std::string& msg) {
            fmt::print(
                stderr,
                fg(fmt::color::red),
                "error:{} {}\n",
                style::RESET,
                msg);
        }

        /// Function to display a selection menu and return the selected index
        int select_from_menu(const std::vector<std::string>& options);

        /// Pauses execution until the user presses ENTER
        void pause(const std::string& message = "Press ENTER to continue...");

        /// Waits for any key press
        void wait_for_key_press(const std::string& message = "Press any key to continue...");

    } // namespace terminal
} // namespace muuk

#endif // MUUK_TERMINAL_HPP
