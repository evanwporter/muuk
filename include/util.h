#ifndef UTILS_H
#define UTILS_H

#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rustify.hpp"

template <typename T>
concept Streamable = requires(std::ostream& os, const T& value) {
    { os << value } -> std::same_as<std::ostream&>;
};

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
    namespace command_line {
        int execute_command(const std::string& command);
        std::string execute_command_get_out(const std::string& command);
        bool command_exists(const std::string& command);
    }

    namespace network {
        Result<nlohmann::json> fetch_json(const std::string& url);
    }

    std::string to_utf8(const std::wstring& wstr);
    bool is_valid_utf8(const std::string& str);

    std::string to_linux_path(const std::string& path, const std::string& prefix = "");
    std::set<std::string> to_linux_path(const std::set<std::string>& paths, const std::string& prefix = "");
    std::string normalize_path(const std::string& path);
    std::vector<std::string> to_linux_path(const std::vector<std::string>& paths, const std::string& prefix = "");

    // std::string normalize_flag(const std::string& flag);
    // std::string normalize_flags(const std::vector<std::string>& flags);

    // template <Streamable T>
    template <Streamable T>
    std::string vectorToString(const std::vector<T>& vec, const std::string& delimiter) {
        std::ostringstream oss;
        if (!vec.empty()) {
            for (size_t i = 0; i < vec.size(); ++i) {
                oss << vec[i];
                if (i < vec.size() - 1) {
                    oss << delimiter;
                }
            }
        }
        return oss.str();
    }

    std::string trim_whitespace(const std::string& str);

    std::string join_strings(const std::vector<std::string>& strings, const std::string& delimiter);

    namespace git {
        std::string get_latest_revision(const std::string& git_url);

        Result<std::string> get_default_branch(const std::string& git_url);
        Result<std::string> get_default_branch(const std::string& author, const std::string& repo);

        Result<nlohmann::json> fetch_repo_tree(const std::string& author, const std::string& repo, const std::string& branch);

        Result<std::vector<std::string>> get_top_level_dirs_of_github(const std::string& author, const std::string& repo);

        Result<std::string> get_license_of_github_repo(const std::string& author, const std::string& repo);

    }

    namespace time {
        int current_year();
    }
} // namespace Utils

#endif
