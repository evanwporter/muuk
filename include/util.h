#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace util {

    // File system utilities
    void ensure_directory_exists(const std::string& dir_path, bool gitignore = false);
    bool path_exists(const std::string& path);
    void remove_path(const std::string& path);
    bool match_pattern(const std::string& path, const std::string& pattern);

    // Download and extraction utilities
    void download_file(const std::string& url, const std::string& output_file);
    void extract_zip(const std::string& archive, const std::string& target_dir);

    // Command execution
    int execute_command(const std::string& command);

    nlohmann::json fetch_json(const std::string& url);

    std::string to_utf8(const std::wstring& wstr);
    bool is_valid_utf8(const std::string& str);

    std::string normalize_path(const std::string& path);
    std::string to_linux_path(const std::string& path);


    bool command_exists(const std::string& command);

    std::string normalize_flag(const std::string& flag);
    std::string normalize_flags(const std::vector<std::string>& flags);

} // namespace Utils

#endif
