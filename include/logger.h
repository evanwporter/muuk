#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

class Logger {
public:
    static void initialize();

    static std::shared_ptr<spdlog::logger> get_logger(const std::string& name);

private:
    static std::shared_ptr<spdlog::logger> global_logger;
    static std::once_flag init_flag;
};

#endif // LOGGER_H
