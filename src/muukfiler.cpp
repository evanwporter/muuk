#include "muukfiler.h"
#include "logger.h"
#include "muukvalidator.hpp"

#include <iostream>
#include <format>
#include <sstream>

MuukFiler::MuukFiler(std::shared_ptr<IFileOperations> file_ops)
    : file_ops_(std::move(file_ops)) {
    config_file_ = file_ops_->get_file_path();
    parse();
}

MuukFiler::MuukFiler(const std::string& config_file)
    : MuukFiler(std::make_shared<FileOperations>(config_file)) {
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

    while (std::getline(file, line)) {
        std::string trimmed = util::trim_whitespace(line);

        if (trimmed.empty() || trimmed[0] == '#') {
            continue;  // Ignore empty lines and comments
        }

        if (trimmed[0] == '[' && trimmed.back() == ']') {
            if (!current_section.empty()) {
                sections_[current_section] = *config_.at_path(current_section).as_table();
                section_data.str("");
                section_data.clear();
            }

            current_section = trimmed.substr(1, trimmed.size() - 2);
            section_order_.push_back(current_section);
        }
        else {
            section_data << line << "\n";
        }
    }

    if (!current_section.empty()) {
        sections_[current_section] = toml::parse(section_data);
    }

    muuk::logger::debug("Final parsed section order:");
    for (const auto& section : section_order_) {
        muuk::logger::debug("  - {}", section);
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

std::string MuukFiler::format_dependencies(const std::unordered_map<std::string, toml::table>& dependencies) {
    std::ostringstream oss;
    oss << "[dependencies]\n";

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

std::ostringstream MuukFiler::generate_default_muuk_toml(
    const std::string& repo_name,
    const std::string& version,
    const std::string& revision,
    const std::string& tag
) {
    std::ostringstream muuk_toml;

    muuk_toml << "[package]\n";
    muuk_toml << "name = \"" << repo_name << "\"\n\n";

    muuk_toml << "[library." << repo_name << "]\n";
    muuk_toml << "include = [\"include/\"]\n";
    muuk_toml << "sources = [\"src/*.cpp\", \"src/*.cc\"]\n";

    return muuk_toml;
}
