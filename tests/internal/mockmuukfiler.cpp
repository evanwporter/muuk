#include "../../include/muukfiler.h"

MockMuukFiler::MockMuukFiler() {
    config_ = toml::table{
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

const toml::table& MockMuukFiler::get_config() const {
    return config_;
}

void MockMuukFiler::set_config(const toml::table& new_config) {
    config_ = new_config;
}

bool MockMuukFiler::has_section(const std::string& section) const {
    return config_.contains(section);
}

toml::table MockMuukFiler::get_section(const std::string& section) const {
    if (has_section(section)) {
        return *config_.get_as<toml::table>(section);
    }
    return {};
}

void MockMuukFiler::update_section(const std::string& section, const toml::table& data) {
    config_.insert_or_assign(section, data);
}


void MockMuukFiler::set_value(const std::string& section, const std::string& key, const toml::node& value) {
    if (!has_section(section)) {
        config_.insert(section, toml::table{});
    }
    auto& section_table = *config_.get_as<toml::table>(section);
    section_table.insert_or_assign(key, value);
}

void MockMuukFiler::remove_key(const std::string& section, const std::string& key) {
    if (has_section(section)) {
        auto& section_table = *config_.get_as<toml::table>(section);
        section_table.erase(key);
    }
}


void MockMuukFiler::remove_section(const std::string& section) {
    config_.erase(section);
}

void MockMuukFiler::append_to_array(const std::string& section, const std::string& key, const toml::node& value) {
    if (!has_section(section)) {
        config_.insert(section, toml::table{});
    }

    auto& section_table = *config_.get_as<toml::table>(section);
    if (!section_table.contains(key) || !section_table[key].is_array()) {
        section_table.insert_or_assign(key, toml::array{});
    }

    section_table[key].as_array()->push_back(value);
}