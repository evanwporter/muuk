#include "../include/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <iostream>

// #define LOG_TO_CONSOLE

std::shared_ptr<spdlog::logger> Logger::global_logger;
std::once_flag Logger::init_flag;

void Logger::initialize() {
    std::call_once(init_flag, []() {
        try {
#ifdef LOG_TO_CONSOLE
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            global_logger = std::make_shared<spdlog::logger>("console_logger", console_sink);
#else
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);
            global_logger = std::make_shared<spdlog::logger>("file_logger", file_sink);
#endif
            global_logger->set_level(spdlog::level::trace);
            global_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
            spdlog::flush_every(std::chrono::seconds(1));
            spdlog::register_logger(global_logger);
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
        });
}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string& name) {
    if (!global_logger) {
        initialize();
    }
    return global_logger;
}
