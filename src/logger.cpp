#include "../include/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <iostream>

std::shared_ptr<spdlog::logger> Logger::global_logger;
std::once_flag Logger::init_flag;

void Logger::initialize() {
    std::call_once(init_flag, []() {
        try {
            // Create a multi-sink logger (file + console)
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/global.log", true);
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            std::vector<spdlog::sink_ptr> sinks{ file_sink, console_sink };
            global_logger = std::make_shared<spdlog::logger>("global_logger", sinks.begin(), sinks.end());

            global_logger->set_level(spdlog::level::trace);
            global_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
            spdlog::flush_every(std::chrono::seconds(1));

            // Register the global logger so it's accessible via spdlog::get()
            spdlog::register_logger(global_logger);
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
        });
}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string& name) {
    if (!global_logger) {
        initialize();  // Ensure logger is initialized
    }
    return global_logger;
}
