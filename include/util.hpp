#ifndef UTILS_H
#define UTILS_H

#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <toml.hpp>

#include "rustify.hpp"

namespace util {

    // ==========================
    //  File System Utilities
    // ==========================
    namespace file_system {
        /// Ensure directory exists and optionally create .gitignore
        void ensure_directory_exists(const std::string& dir_path, const bool gitignore = false);

        /// Check if a path exists
        bool path_exists(const std::string& path);

        std::string to_linux_path(const std::string& path, const std::string& prefix = "");
        std::set<std::string> to_linux_path(const std::set<std::string>& paths, const std::string& prefix = "");
        std::vector<std::string> to_linux_path(const std::vector<std::string>& paths, const std::string& prefix = "");

        std::string sanitize_path(const std::string& input);

        std::string escape_drive_letter(const std::string& path);
    }

    // ==========================
    //  Command Line Utilities
    // ==========================
    namespace command_line {
        /// Execute a command and return the output as a string
        std::string execute_command_get_out(const std::string& command);
        bool command_exists(const std::string& command);

        /// Execute a command and return the exit code
        int execute_command(const std::string& command);

        /// Execute a command with nice formatting of arguments
        template <typename... Args>
        int execute_command(fmt::format_string<Args...> fmt_str, Args&&... args) {
            std::string command = fmt::format(fmt_str, std::forward<Args>(args)...);
            return execute_command(command);
        }
    }

    // ==========================
    //  Network Utilities
    // ==========================
    namespace network {
        Result<nlohmann::json> fetch_json(const std::string& url);
        Result<void> download_file(const std::string& url, const std::string& output_file);
    }

    std::string trim_whitespace(const std::string& str);

    // ==========================
    //  Git Utilities
    // ==========================
    namespace git {
        std::string get_latest_revision(const std::string& git_url);

        Result<std::string> get_default_branch(const std::string& git_url);
        Result<std::string> get_default_branch(const std::string& author, const std::string& repo);

        Result<nlohmann::json> fetch_repo_tree(const std::string& author, const std::string& repo, const std::string& branch);

        Result<std::vector<std::string>> get_top_level_dirs_of_github(const std::string& author, const std::string& repo);

        Result<std::string> get_license_of_github_repo(const std::string& author, const std::string& repo);

    }

    // ==========================
    //  Time Utilities
    // ==========================
    namespace time {
        int current_year();
    }

    // ==========================
    //  Array Utilities
    // ==========================
    namespace array_ops {
        /// Merge std::vector with another std::vector
        template <typename T>
        inline void merge(std::vector<T>& dest, const std::vector<T>& src) {
            dest.insert(dest.end(), src.begin(), src.end());
        }

        /// Merge std::unordered_set with another std::unordered_set
        template <typename T>
        inline void merge(std::unordered_set<T>& dest, const std::unordered_set<T>& src) {
            dest.insert(src.begin(), src.end());
        }
    }

    /// Checks if a string is a valid positive integer
    bool is_integer(const std::string& s);
} // namespace Utils

#endif
