#include "../include/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <iostream>

// Static member definitions
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
std::once_flag Logger::init_flag;

void Logger::initialize() {
    std::call_once(init_flag, []() {
        try {
            // Ensure the "logs" directory exists
            std::filesystem::create_directories("logs");

            // Set up file sink (logs everything)
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

            // Set up console sink (only warnings and errors)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::warn);
            console_sink->set_pattern("[%l] %v");

            // Create single global logger
            logger_ = std::make_shared<spdlog::logger>("muuk", spdlog::sinks_init_list{ file_sink, console_sink });
            logger_->set_level(spdlog::level::trace);

            spdlog::flush_every(std::chrono::seconds(1));
            spdlog::set_default_logger(logger_); // Set as default logger

            logger_->info("Logger initialized successfully.");
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
            std::exit(EXIT_FAILURE);  // Prevent silent crash
        }
        });
}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string& name) {
    if (!logger_) {
        initialize();
    }
    return logger_;
}

void Logger::log_important_info(const std::string& message) {
    std::cout << message << std::endl;
    get_logger()->info(message);
}