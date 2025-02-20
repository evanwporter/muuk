#include "muukfiler.h"
#include "logger.h"
#include <iostream>
#include <format>

MuukFiler::MuukFiler(const std::string& config_file) : config_file_(config_file) {
    logger_ = logger::get_logger("muuk::filer");

    parse_file();

    // section_order_ = parse_section_order();

    if (!config_file.ends_with("lock.toml")) validate_muuk();
}

void MuukFiler::parse_file() {
    std::ifstream file(config_file_);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + config_file_);
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
                sections_[current_section] = toml::parse(section_data);
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

    file.close();

    logger_->debug("Final parsed section order:");
    for (const auto& section : section_order_) {
        logger_->debug("  - {}", section);
    }
}

toml::table& MuukFiler::get_section(const std::string& section) {
    if (sections_.find(section) == sections_.end()) {
        sections_[section] = toml::table{};
        section_order_.push_back(section);
    }
    return sections_.at(section);  // Return a reference
}


void MuukFiler::modify_section(const std::string& section, const toml::table& data) {
    if (sections_.find(section) == sections_.end()) {
        section_order_.push_back(section);
    }
    sections_[section] = data;
}

void MuukFiler::write_to_file() {
    std::ofstream file(config_file_, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + config_file_);
    }

    for (const auto& section : section_order_) {
        file << "[" << section << "]\n";
        file << sections_.at(section) << "\n\n";
    }

    file.close();
}

void MuukFiler::validate_muuk() {
    // if (sections_.find("package") == sections_.end()) {
    //     throw std::runtime_error("Missing required [package] section.");
    // }

    // const auto& package_section = sections_["package"];
    // if (!package_section.contains("name") || !package_section["name"].is_string()) {
    //     throw std::runtime_error("[package] must have a 'name' key of type string.");
    // }

    // std::string package_name = *package_section["name"].value<std::string>();
    // if (sections_.find("library." + package_name) == sections_.end()) {
    //     throw std::runtime_error("Missing required [library." + package_name + "] section.");
    // }
}

toml::table MuukFiler::get_config() const {
    return toml::parse_file(config_file_);
}

bool MuukFiler::has_section(const std::string& section) const {
    return sections_.find(section) != sections_.end();
}

std::vector<std::string> MuukFiler::parse_section_order() {
    std::vector<std::string> parsed_order;

    if (!logger_) {
        std::cout << "HELP" << std::endl;
        return parsed_order;
    }

    logger_->debug("MuukFiler initialized with config file: {}", config_file_);

    if (config_file_.empty()) {
        logger_->error("[MuukFiler] Config file path is empty!");
        return parsed_order;
    }

    std::ifstream file(config_file_);
    if (!file.is_open()) {
        logger_->error("[MuukFiler] Failed to open TOML file for reading: {}", config_file_);
        return parsed_order;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = util::trim_whitespace(line);

        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            parsed_order.push_back(section);
        }
    }

    file.close();

    logger_->info("[MuukFiler] Parsed section order:");
    for (const auto& section : parsed_order) {
        logger_->info("  {}", section);
    }

    return parsed_order;
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

