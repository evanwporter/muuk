#include "../include/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <iostream>

std::shared_ptr<spdlog::logger> Logger::global_logger;
std::once_flag Logger::init_flag;

void Logger::initialize() {
    std::call_once(init_flag, []() {
        try {
            // Set up a file sink ONLY (no console output)
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);

            // Create a logger with only the file sink
            global_logger = std::make_shared<spdlog::logger>("file_logger", file_sink);

            // Set log level (change to spdlog::level::info for fewer logs)
            global_logger->set_level(spdlog::level::trace);

            // Log format: [Timestamp] [Level] Message
            global_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

            // Ensure logs are flushed every second
            spdlog::flush_every(std::chrono::seconds(1));

            // Register logger globally
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
