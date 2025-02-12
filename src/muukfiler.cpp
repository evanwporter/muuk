#include "../include/muukfiler.h"
#include "../include/logger.h"
#include <fstream>
#include <filesystem>
#include "muukfiler.h"

namespace fs = std::filesystem;

MuukFiler::MuukFiler(const std::string& config_file) : config_file_(config_file) {
    logger_ = Logger::get_logger("muuk_logger");
    load_config();

    if (!config_file_.ends_with("muuk.lock.toml")) validate_muuk();
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
    return has_section(section) ? *config_.get_as<toml::table>(section) : toml::table{};
}

void MuukFiler::update_section(const std::string& section, const toml::table& data) {
    config_.insert_or_assign(section, data);
    track_section_order(section);
    save_config();
}

void MuukFiler::set_value(const std::string& section, const std::string& key, const toml::node& value) {
    ensure_section_exists(section);
    config_.at(section).as_table()->insert_or_assign(key, value);
    save_config();
}

void MuukFiler::remove_key(const std::string& section, const std::string& key) {
    if (has_section(section)) {
        config_.at(section).as_table()->erase(key);
        save_config();
    }
}

void MuukFiler::remove_section(const std::string& section) {
    if (config_.erase(section)) {
        section_order_.erase(std::remove(section_order_.begin(), section_order_.end(), section), section_order_.end());
        save_config();
    }
}

void MuukFiler::append_to_array(const std::string& section, const std::string& key, const toml::node& value) {
    ensure_section_exists(section);
    auto& section_table = *config_.at(section).as_table();

    if (!section_table.contains(key) || !section_table[key].is_array()) {
        section_table.insert_or_assign(key, toml::array{});
    }

    section_table[key].as_array()->push_back(value);
    save_config();
}

void MuukFiler::load_config() {
    if (!fs::exists(config_file_)) {
        std::ofstream(config_file_) << get_default_config();
    }

    try {
        config_ = toml::parse_file(config_file_);
        track_section_order();
    }
    catch (const toml::parse_error& e) {
        throw std::runtime_error(format_error("Error parsing TOML: " + std::string(e.what())));
    }
}

void MuukFiler::save_config() {
    std::ofstream(config_file_) << config_;
}

void MuukFiler::ensure_section_exists(const std::string& section) {
    if (!has_section(section)) {
        config_.insert_or_assign(section, toml::table{});
        track_section_order(section);
    }
}

void MuukFiler::track_section_order(const std::string& section) {
    if (std::find(section_order_.begin(), section_order_.end(), section) == section_order_.end()) {
        section_order_.push_back(section);
    }
}

std::string MuukFiler::format_error(const std::string& error_message) {
    return "[Error] " + error_message + "\n[Config File]: " + config_file_;
}

void MuukFiler::validate_muuk() {
    if (!has_section("package")) {
        throw std::runtime_error(format_error("Missing required [package] section."));
    }

    const auto& package_section = get_section("package");
    if (!package_section.contains("name") || !package_section["name"].is_string()) {
        throw std::runtime_error(format_error("[package] must have a 'name' key of type string."));
    }

    std::string package_name = *package_section["name"].value<std::string>();
    const auto& library_section = get_section("library");

    if (!library_section.contains(package_name) || !library_section.at(package_name).is_table()) {
        throw std::runtime_error(format_error("Missing required [library." + package_name + "] section."));
    }

    auto& library_package = *library_section.at(package_name).as_table();
    validate_array_of_strings(library_package, "include", true);
    validate_array_of_strings(library_package, "sources");
    validate_array_of_strings(library_package, "lflags");
    validate_array_of_strings(library_package, "libflags");

    if (library_package.contains("dependencies")) {
        auto& dependencies = *library_package.at("dependencies").as_table();
        for (const auto& [dep_name, dep_value] : dependencies) {
            if (!dep_value.is_string() && (!dep_value.is_table() || !dep_value.as_table()->contains("version"))) {
                throw std::runtime_error(format_error("Dependency '" + std::string(dep_name.str()) + "' must have a valid 'version'."));
            }
        }
    }

    if (has_section("clean")) {
        validate_array_of_strings(get_section("clean"), "patterns");
    }
}

void MuukFiler::validate_array_of_strings(const toml::table& section, const std::string& key, bool required) {
    if (!section.contains(key)) {
        if (required) throw std::runtime_error(format_error("Missing required key [" + key + "] in section."));
        return;
    }

    if (!section.at(key).is_array()) {
        throw std::runtime_error(format_error("[" + key + "] must be an array."));
    }

    for (const auto& item : *section.at(key).as_array()) {
        if (!item.is_string()) {
            throw std::runtime_error(format_error("[" + key + "] must be an array of strings."));
        }
    }
}

toml::table MuukFiler::get_default_config() const
{
    return toml::table{};
}
