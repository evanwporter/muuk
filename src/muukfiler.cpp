#include "../include/muukfiler.h"
#include "../include/logger.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

MuukFiler::MuukFiler(const std::string& config_file) : config_file_(config_file) {
    config_ = load_or_create_config();
    logger_ = Logger::get_logger("muuk_logger");

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
    config_stream << config_;
    config_stream.close();
}


bool MuukFiler::contains_key(toml::table& table, std::string& key) {
    return table.contains(key);
}

std::string MuukFiler::format_error(const std::string& error_message) {
    std::ostringstream oss;
    oss << "[Error] " << error_message
        << "\n[Config File]: " << config_file_
        << "\n[Current muuk.toml Configuration]:\n"
        << config_;
    return oss.str();
}

void MuukFiler::validate_muuk() {
    logger_->info("[muuk::validate] Validating {} structure...", config_file_);

    if (!has_section("package")) {
        throw std::runtime_error(format_error("Missing required [package] section."));
    }

    const toml::table& package_section = get_section("package");
    if (!package_section.contains("name") || !package_section["name"].is_string()) {
        throw std::runtime_error("Invalid TOML: [package] section must have a 'name' key of type string.");
    }

    std::string package_name = *package_section["name"].value<std::string>();

    // Check for required [library.{package.name}] section
    std::string library_section_name = "library";
    if (!has_section(library_section_name)) {
        throw std::runtime_error("Invalid TOML: Missing required [library] section.");
    }

    const toml::table& library_section = get_section(library_section_name);
    if (!library_section.contains(package_name) || !library_section.at(package_name).is_table()) {
        throw std::runtime_error("Invalid TOML: Missing required [library." + package_name + "] section.");
    }

    // Get the library.{package.name} subtable safely
    const toml::table& library_package_section = *library_section.at(package_name).as_table();

    // Ensure library.include exists and is an array of strings (Required)
    validate_array_of_strings(library_package_section, "include");

    // Validate other fields if present
    validate_array_of_strings(library_package_section, "sources");
    validate_array_of_strings(library_package_section, "lflags");
    validate_array_of_strings(library_package_section, "libflags");

    // Validate dependencies section
    if (library_package_section.contains("dependencies")) {
        const auto& dependencies = library_package_section.at("dependencies");
        if (!dependencies.is_table()) {
            throw std::runtime_error("Invalid TOML: [dependencies] must be a table.");
        }

        for (const auto& [dep_name, dep_value] : *dependencies.as_table()) {
            if (dep_value.is_string()) {
                logger_->info("Dependency '{}' has version '{}'", dep_name.str(), *dep_value.value<std::string>());
            }
            else if (dep_value.is_table()) {
                const toml::table& dep_table = *dep_value.as_table();
                if (!dep_table.contains("version") || !dep_table.at("version").is_string()) {
                    throw std::runtime_error("Invalid TOML: Dependency '" + std::string(dep_name.str()) + "' must contain a 'version' key of type string.");
                }
                logger_->info("Dependency '{}' has version '{}'", dep_name.str(), *dep_table.at("version").value<std::string>());
            }
            else {
                throw std::runtime_error("Invalid TOML: Dependency '" + std::string(dep_name.str()) + "' must be a string or a table with a 'version' key.");
            }
        }
    }

    // Validate [clean] section
    if (has_section("clean")) {
        const toml::table& clean_section = get_section("clean");
        validate_array_of_strings(clean_section, "patterns");
    }

    logger_->info("[muuk::validate] muuk.toml validation successful!");
}

void MuukFiler::validate_array_of_strings(const toml::table& section, const std::string& key, bool required) {
    if (!section.contains(key)) {
        if (required) {
            throw std::runtime_error("Invalid TOML: Missing required key [" + key + "] in section.");
        }
        return;
    }

    const toml::array* value = section.at(key).as_array();
    if (!value) {
        throw std::runtime_error("Invalid TOML: [" + key + "] must be an array.");
    }

    for (const auto& item : *value) {
        if (!item.is_string()) {
            throw std::runtime_error("Invalid TOML: [" + key + "] must be an array of strings.");
        }
    }
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
