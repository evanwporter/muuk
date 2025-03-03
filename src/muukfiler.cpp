#include "muukfiler.h"
#include "logger.h"
#include "muukvalidator.hpp"

#include <iostream>
#include <format>
#include <sstream>

MuukFiler::MuukFiler(std::shared_ptr<IFileOperations> file_ops, bool is_lock_file_)
    : file_ops_(std::move(file_ops)), is_lock_file(is_lock_file_) {
    config_file_ = file_ops_->get_file_path();
    parse();
}

MuukFiler::MuukFiler(const std::string& config_file, bool is_lock_file)
    : MuukFiler(std::make_shared<FileOperations>(config_file), is_lock_file) {
}

Result<MuukFiler> MuukFiler::create(std::shared_ptr<IFileOperations> file_ops, bool is_lock_file) {
    if (!file_ops) {
        return Err("Invalid file operations provided.");
    }

    std::string config_file = file_ops->get_file_path();
    if (config_file.empty()) {
        return Err("Error: Config file not specified.");
    }

    if (!file_ops->exists()) {
        return Err("Failed to open file: {}", config_file);
    }

    MuukFiler filer(std::move(file_ops), is_lock_file);

    auto result = muuk::validate_muuk_toml(filer.get_config());
    if (!result) {
        muuk::logger::error(result.error());
        return Err("");
    }

    return filer;
}

Result<MuukFiler> MuukFiler::create(const std::string& config_file, bool is_lock_file) {
    return create(std::make_shared<FileOperations>(config_file), is_lock_file);
}

void MuukFiler::parse() {
    if (!file_ops_->exists()) {
        throw std::runtime_error("Failed to open file: " + file_ops_->get_file_path());
    }

    std::string content = file_ops_->read_file();
    std::istringstream file(content);

    // TODO: don't do this since its unnecessary parsing
    // slows down the process
    try {
        config_ = toml::parse(content);
        muuk::validate_muuk_toml(config_);
    }
    catch (const toml::parse_error& e) {
        muuk::fmt_rt_err("TOML Parsing Error: {}", e.what());
    }
    catch (const muuk::invalid_toml& e) {
        muuk::fmt_rt_err("TOML Validation Error: {}", e.what());
    }

    std::string line, current_section;
    std::stringstream section_data;
    bool new_section_started = false;

    while (std::getline(file, line)) {
        std::string trimmed = util::trim_whitespace(line);

        if (trimmed.empty() || trimmed[0] == '#') {
            continue;  // Ignore empty lines and comments
        }

        // Detecting a new section
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            // Save previous section data
            if (!current_section.empty()) {
                sections_[current_section] = toml::parse(section_data.str());
                section_data.str("");
                section_data.clear();
            }

            current_section = trimmed.substr(1, trimmed.size() - 2);
            section_order_.push_back(current_section);
            new_section_started = true;

            // Ensure each section starts fresh
            continue;
        }

        if (new_section_started) {
            section_data.str("");  // Reset section data
            section_data.clear();
            new_section_started = false;
        }

        section_data << line << "\n";  // Append data for the current section
    }

    // Save last section
    if (!current_section.empty()) {
        sections_[current_section] = toml::parse(section_data.str());
    }

    // Logging to verify correct sections
    muuk::logger::info("Final parsed sections:");
    for (const auto& section : sections_) {
        muuk::logger::info("  - {}", section.first);
    }
}

toml::table& MuukFiler::get_section(const std::string& section) {
    if (sections_.find(section) == sections_.end()) {
        sections_[section] = toml::table{};
        section_order_.push_back(section);
    }
    return sections_.at(section);
}

void MuukFiler::modify_section(const std::string& section, const toml::table& data) {
    if (sections_.find(section) == sections_.end()) {
        section_order_.push_back(section);
        muuk::logger::info("Added new section: {}", section);
    }
    sections_[section] = data;
}

void MuukFiler::write_to_file() {
    std::ostringstream content;
    for (const auto& section : section_order_) {
        content << "[" << section << "]\n";
        content << sections_.at(section) << "\n\n";
    }
    file_ops_->write_file(content.str());
}

// TODO: Make it not dependent on config_file_
toml::table MuukFiler::get_config() const {
    if (!std::filesystem::exists(config_file_)) {
        throw std::runtime_error("Error: Config file not found: " + config_file_);
    }
    return toml::parse_file(config_file_);
}

bool MuukFiler::has_section(const std::string& section) const {
    return sections_.find(section) != sections_.end();
}

std::string MuukFiler::format_dependencies(const std::unordered_map<std::string, toml::table>& dependencies, std::string section_name) {
    std::ostringstream oss;
    oss << "[" << section_name << "]\n";

    for (const auto& [dep_name, dep_info] : dependencies) {
        oss << dep_name << " = { ";

        std::string dep_entries;
        bool first = true;

        for (const auto& [key, val] : dep_info) {
            if (!first) dep_entries += ", ";
            dep_entries += std::format("{} = '{}'", std::string(key.str()), *val.value<std::string>());
            first = false;
        }

        oss << std::format("{} }}\n", dep_entries);
    }

    oss << "\n";
    return oss.str();
}