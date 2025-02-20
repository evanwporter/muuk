#include "logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <iostream>

// Static member definitions
std::shared_ptr<spdlog::logger> logger::logger_ = nullptr;
std::once_flag logger::init_flag;

void logger::initialize() {
    std::call_once(init_flag, []() {
        try {
            std::filesystem::create_directories("logs");

            // File sink (logs to file)
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v"); // Standard file log format

            // Console sink (logs to console with colors and custom formatting)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::warn);

            console_sink->set_pattern("%^[%l]%$ %v");  // Colors levels based on severity

            logger_ = std::make_shared<spdlog::logger>("muuk", spdlog::sinks_init_list{ file_sink, console_sink });
            logger_->set_level(spdlog::level::trace);

            logger_->set_error_handler([](const std::string& msg) {
                std::cerr << "\033[1;31merror:\033[0m " << msg << std::endl;
                std::exit(EXIT_FAILURE);
                });

            spdlog::flush_every(std::chrono::seconds(1));
            spdlog::set_default_logger(logger_);

            logger_->info("Logger initialized successfully.");
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        });
}

std::shared_ptr<spdlog::logger> logger::get_logger(const std::string& name) {
    (void)name;
    initialize();
    return logger_;
}
