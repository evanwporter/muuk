#ifndef MUUK_FILER_H
#define MUUK_FILER_H

#include "logger.h"
#include "util.h"

#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

class MuukFiler {
public:
    explicit MuukFiler(const std::string& config_file);

    toml::table& get_section(const std::string& section);
    void modify_section(const std::string& section, const toml::table& data);
    void write_to_file();
    void validate_muuk();

    toml::table get_config() const;
    bool has_section(const std::string& section) const;

    std::vector<std::string> parse_section_order();

private:
    std::shared_ptr<spdlog::logger> logger_;

    std::string config_file_;
    std::unordered_map<std::string, toml::table> sections_;

    std::vector<std::string> section_order_;

    void parse_file();
};

#endif // MUUK_FILER_H
