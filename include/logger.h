#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

class Logger {
public:
    static std::shared_ptr<spdlog::logger> get_logger(const std::string& name = "");
    static void log_important_info(const std::string& message);
    static void initialize();


private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::once_flag init_flag;
};

#endif // LOGGER_H
