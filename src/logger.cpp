#include "logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <format>

namespace muuk {

    std::once_flag logger::init_flag;
    std::shared_ptr<spdlog::logger> logger::logger_;

    void logger::initialize() {
        std::call_once(init_flag, []() {

            // File sink (logs to file)
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/muuk.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

            // // Console sink (logs to console)
            // auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            // console_sink->set_level(spdlog::level::warn); // Only log warnings and above
            // console_sink->set_pattern("%^%l%$ %v");

            // logger_ = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{ file_sink, console_sink });
            logger_ = std::make_shared<spdlog::logger>("multi_sink", file_sink);

#ifdef DEBUG
            logger_->set_level(spdlog::level::trace); // Set logger level to trace
#else
            logger_->set_level(spdlog::level::info); // Set logger level to info
#endif

            spdlog::set_default_logger(logger_);

            spdlog::flush_on(spdlog::level::warn);
            spdlog::flush_every(std::chrono::seconds(1));
            });
    }
} // namespace muuk