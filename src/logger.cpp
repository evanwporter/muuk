#include "../include/logger.h"
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

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::warn);
            console_sink->set_pattern("[%l] %v");

            logger_ = std::make_shared<spdlog::logger>("muuk", spdlog::sinks_init_list{ file_sink, console_sink });
            logger_->set_level(spdlog::level::trace);

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
    initialize();

    return logger_;
}

void logger::log_important_info(const std::string& message) {
    std::cout << message << std::endl;
    get_logger()->info(message);
}

void logger::error(const std::string& message, const std::string& help) {
    std::string error_prefix = "\033[1;31merror:\033[0m ";
    std::cerr << error_prefix << message << "\n";
    if (!help.empty()) {
        std::cerr << "  \033[1;34mhelp:\033[0m " << help << "\n";
    }
}