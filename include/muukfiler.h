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
    MuukFiler();
    explicit MuukFiler(const std::string& config_file);
    explicit MuukFiler(const std::string& config_content, bool is_content);

    toml::table& get_section(const std::string& section);
    void modify_section(const std::string& section, const toml::table& data);
    void write_to_file();
    void validate_muuk();

    toml::table get_config() const;
    bool has_section(const std::string& section) const;

    std::vector<std::string> parse_section_order();

    static std::string format_dependencies(const std::unordered_map<std::string, toml::table>& dependencies);

    static std::ostringstream generate_default_muuk_toml(
        const std::string& repo_name,
        const std::string& version = "",
        const std::string& revision = "",
        const std::string& tag = ""
    );

private:
    std::shared_ptr<spdlog::logger> logger_;

    std::string config_file_;
    std::unordered_map<std::string, toml::table> sections_;

    std::vector<std::string> section_order_;

    void parse_file();
    void parse_content(const std::string& config_content);

    void parse(std::istream& config_content);
};

#endif // MUUK_FILER_H
