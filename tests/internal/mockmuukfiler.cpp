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
