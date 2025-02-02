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

    if (std::find(section_order_.begin(), section_order_.end(), section) == section_order_.end()) {
        section_order_.push_back(section);
    }

    save_config();
}

void MuukFiler::set_value(const std::string& section, const std::string& key, const toml::node& value) {
    if (!has_section(section)) {
        config_.insert(section, toml::table{});
    }

    auto& section_table = *config_.get_as<toml::table>(section);
    section_table.insert_or_assign(key, value);

    save_config();
}

void MuukFiler::remove_key(const std::string& section, const std::string& key) {
    if (has_section(section)) {
        auto& section_table = *config_.get_as<toml::table>(section);
        section_table.erase(key);
        save_config();
    }
}

void MuukFiler::remove_section(const std::string& section) {
    if (config_.contains(section)) {
        config_.erase(section);

        section_order_.erase(
            std::remove(section_order_.begin(), section_order_.end(), section),
            section_order_.end()
        );

        save_config();
    }
}

void MuukFiler::append_to_array(const std::string& section, const std::string& key, const toml::node& value) {
    if (!has_section(section)) {
        config_.insert(section, toml::table{});
        section_order_.push_back(section);
    }

    auto& section_table = *config_.get_as<toml::table>(section);
    if (!section_table.contains(key) || !section_table[key].is_array()) {
        section_table.insert_or_assign(key, toml::array{});
    }

    section_table[key].as_array()->push_back(value);
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
        toml::table parsed_config = toml::parse_file(config_file_);
        config_ = parsed_config;

        track_section_order();

        return parsed_config;
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

    for (const auto& section : section_order_) {
        if (!config_.contains(section)) continue;  // Skip deleted sections

        config_stream << "[" << section << "]\n";
        auto& tbl = *config_.get_as<toml::table>(section);
        for (const auto& [key, value] : tbl) {
            if (value.is_string()) {
                config_stream << key << " = \"" << *value.value<std::string>() << "\"\n";
            }
            else if (value.is_integer()) {
                config_stream << key << " = " << *value.value<int64_t>() << "\n";
            }
            else if (value.is_floating_point()) {
                config_stream << key << " = " << *value.value<double>() << "\n";
            }
            else if (value.is_boolean()) {
                config_stream << key << " = " << (*value.value<bool>() ? "true" : "false") << "\n";
            }
            else if (value.is_array()) {
                config_stream << key << " = [";
                auto& arr = *value.as_array();
                bool first = true;
                for (const auto& item : arr) {
                    if (!first) config_stream << ", ";
                    first = false;
                    if (item.is_string()) {
                        config_stream << "\"" << *item.value<std::string>() << "\"";
                    }
                    else if (item.is_integer()) {
                        config_stream << *item.value<int64_t>();
                    }
                    else if (item.is_floating_point()) {
                        config_stream << *item.value<double>();
                    }
                    else if (item.is_boolean()) {
                        config_stream << (*item.value<bool>() ? "true" : "false");
                    }
                }
                config_stream << "]\n";
            }
        }
        config_stream << "\n";  // Add newline between sections
    }

    config_stream.close();
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

void MuukFiler::track_section_order() {
    section_order_.clear();

    for (const auto& [key, value] : config_) {
        section_order_.push_back(std::string(key.str()));
    }
}
