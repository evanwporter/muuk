#include "../include/muukfiler.h"
#include "../include/logger.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

MuukFiler::MuukFiler(const std::string& config_file) : config_file_(config_file) {
    config_ = load_or_create_config();
    logger_ = Logger::get_logger("muuk_logger");

}

const toml::table& MuukFiler::get_config() const {
    return config_;
}

void MuukFiler::set_config(const toml::table& new_config) {
    config_ = new_config;
    save_config();
}

bool MuukFiler::has_section(const std::string& section) const {
    return config_.contains(section);
}

toml::table MuukFiler::get_section(const std::string& section) const {
    logger_->info("Getting section {}", section);
    if (has_section(section)) {
        return *config_.get_as<toml::table>(section);
    }
    return {};
}

void MuukFiler::update_section(const std::string& section, const toml::table& data) {
    config_.insert_or_assign(section, data);
    save_config();
}

toml::table MuukFiler::load_or_create_config() {
    if (!fs::exists(config_file_)) {
        std::ofstream config_stream(config_file_);
        if (!config_stream) {
            throw std::runtime_error("Unable to create config file: " + config_file_);
        }
        config_stream << get_default_config();
        config_stream.close();
    }

    try {
        return toml::parse_file(config_file_);
    }
    catch (const toml::parse_error& e) {
        throw std::runtime_error("Error parsing TOML from config file: " + std::string(e.what()));
    }
}

void MuukFiler::save_config() {
    std::ofstream config_stream(config_file_);
    if (!config_stream) {
        throw std::runtime_error("Unable to write to config file: " + config_file_);
    }
    config_stream << config_;
}

toml::table MuukFiler::get_default_config() const {
    return toml::table{
        {"scripts", toml::table{
            {"greet", "echo Hello, World!"},
            {"list", toml::array{"echo Listing files...", "dir"}}
        }},
        {"clean", toml::array{"*.o", "*.pyd"}},
        {"build", toml::table{
            {"source_files", toml::array{"main.cpp"}},
            {"output", "main.exe"},
            {"flags", toml::array{"/EHsc"}},
            {"defines", toml::array{"DEBUG"}},
            {"include_dirs", toml::array{"include"}},
            {"extra_args", toml::array{}}
        }}
    };
}
