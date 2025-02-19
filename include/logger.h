#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <format>
#include <iostream>


class logger {
public:
    static std::shared_ptr<spdlog::logger> get_logger(const std::string& name = "");
    static void log_important_info(const std::string& message);
    static void initialize();

    template<typename... Args>
    static void error(const std::string& format_str, Args&&... args) {
        std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));

        std::string error_prefix = "\033[1;31merror:\033[0m ";
        std::cerr << error_prefix << formatted_message << "\n";

        get_logger()->error(formatted_message);
    }

    template<typename... Args>
    static void warning(const std::string& format_str, Args&&... args) {
        std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));

        std::string warning_prefix = "\033[1;33mwarning:\033[0m ";
        std::cerr << warning_prefix << formatted_message << "\n";

        get_logger()->warn(formatted_message);
    }

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::once_flag init_flag;
};

#endif // LOGGER_H
