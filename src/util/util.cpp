#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "logger.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace util {

    // ==========================
    //  File System Utilities
    // ==========================
    namespace file_system {
        void ensure_directory_exists(const std::string& dir_path, const bool gitignore) {
            if (!fs::exists(dir_path)) {
                fs::create_directories(dir_path);
                muuk::logger::info("Created directory: {}", dir_path);
            } else {
                muuk::logger::debug("Directory already exists: {}", dir_path);
            }

            if (gitignore) {
                std::string gitignore_file = dir_path + "/.gitignore";
                if (!fs::exists(gitignore_file)) {
                    std::ofstream gitignore_stream(gitignore_file);
                    if (gitignore_stream.is_open()) {
                        gitignore_stream << "*\n";
                        gitignore_stream.close();
                        muuk::logger::info("Created .gitignore file in directory: {}", dir_path);
                    } else {
                        muuk::logger::error("Failed to create .gitignore file in directory: {}", dir_path);
                    }
                }
            }
        }

        bool path_exists(const std::string& path) {
            bool exists = fs::exists(path);
            muuk::logger::debug("Checked existence of '{}': {}", path, exists);
            return exists;
        }

        std::vector<std::string> to_linux_path(const std::vector<std::string>& paths, const std::string& prefix) {
            std::vector<std::string> new_paths;
            new_paths.reserve(paths.size()); // Reserve space for efficiency

            std::transform(paths.begin(), paths.end(), std::back_inserter(new_paths), [&prefix](const std::string& path) { return to_linux_path(path, prefix); });

            return new_paths;
        }

        std::set<std::string> to_linux_path(const std::set<std::string>& paths, const std::string& prefix) {
            std::set<std::string> new_paths;
            for (const auto& path : paths) {
                new_paths.insert(to_linux_path(path, prefix));
            }
            return new_paths;
        }

        std::string to_linux_path(const std::string& path, const std::string& prefix) {
            std::string new_path = path;
            std::replace(new_path.begin(), new_path.end(), '\\', '/');

            // Check if the path is absolute
            bool is_absolute = fs::path(new_path).is_absolute();

            return (is_absolute ? "" : prefix) + new_path;
        }

        /// Modify Windows drive letter (e.g., C:) to a format that can be used in build files (e.g., C$:)
        std::string escape_drive_letter(const std::string& path) {
            std::string new_path = path;
#ifdef _WIN32
            // Check if path has a drive letter (e.g., C:\)
            if (new_path.length() > 2 && new_path[1] == ':') {
                new_path = new_path.substr(0, 1) + "$:" + new_path.substr(2);
            }
#endif
            return new_path;
        }
    } // namespace file_system

    // ==========================
    //  Command Line Utilities
    // ==========================
    namespace command_line {
        int execute_command(const std::string& command) {
            muuk::logger::info("Executing command: {}", command);
            // if (!command_exists(command)) {
            //     muuk::logger::error("Command does not exist: {}", command);
            //     return -1;
            // }
            return system(command.c_str());
        }

        std::string execute_command_get_out(const std::string& command) {

            // if (!command_exists(command)) {
            //     muuk::logger::error("Command does not exist: {}", command);
            //     return "";
            // }

            muuk::logger::info("Executing command: {}", command);

            std::array<char, 128> buffer;
            std::string result;

#ifdef _WIN32
            std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif

            if (!pipe) {
                muuk::logger::error("Failed to execute command: {}", command);
                throw std::runtime_error("Failed to execute command: " + command);
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            muuk::logger::info("Command output:\n{}", result);
            return result;
        }

        bool command_exists(const std::string& full_command) {
            // Extract only the first word (program name)
            size_t space_pos = full_command.find(' ');
            std::string base_command = (space_pos == std::string::npos)
                ? full_command
                : full_command.substr(0, space_pos);

            muuk::logger::info("Checking if command exists: '{}'", base_command);

#ifdef _WIN32
            std::string check_command = "where " + base_command + " >nul 2>&1";
#else
            std::string check_command = "which " + base_command + " >/dev/null 2>&1";
#endif

            bool exists = (std::system(check_command.c_str()) == 0);

            if (!exists) {
                muuk::logger::error("Command not found: '{}'", base_command);
            }

            return exists;
        }
    } // namespace command_line

    // ==========================
    //  Network Utilities
    // ==========================
    namespace network {

        Result<nlohmann::json> fetch_json(const std::string& url) {
            std::string output_file = "github_api_response.json";
            std::string command = "wget --quiet -O - "
                                  "--header=\"Accept: application/vnd.github.v3+json\" "
                                  "--header=\"User-Agent: Mozilla/5.0\" "
                                  "--no-check-certificate "
                + url;

            std::string result = command_line::execute_command_get_out(command);

            try {
                return nlohmann::json::parse(result);
            } catch (const std::exception& e) {
                return Err("JSON parsing failed: " + std::string(e.what()));
            }
        }

        Result<void> download_file(const std::string& url, const std::string& output_path) {
            std::string command;

            if (util::command_line::command_exists("wget")) {
                command = "wget --quiet --output-document=" + output_path + " --no-check-certificate " + url;
            } else if (util::command_line::command_exists("curl")) {
                command = "curl -L -o " + output_path + " " + url;
            } else {
                muuk::logger::error("Neither wget nor curl is available on the system.");
                throw std::runtime_error("No suitable downloader found. Install wget or curl.");
            }

            muuk::logger::info("Executing download command: {}", command);

            int result = util::command_line::execute_command(command.c_str());
            if (result != 0) {
                return Err("File download failed with exit code: " + std::to_string(result));
            }

            return {};
        }
    } // namespace network

    std::string trim_whitespace(const std::string& str) {
        auto start = str.begin();
        while (start != str.end() && std::isspace(*start)) {
            ++start;
        }

        auto end = str.end();
        do {
            --end;
        } while (end > start && std::isspace(*end));

        return std::string(start, end + 1);
    }

    // ==========================
    //  Time Utilities
    // ==========================
    namespace time {

        int current_year() {
            std::time_t t = std::time(nullptr);
            std::tm time_info {};

#ifdef _WIN32
            localtime_s(&time_info, &t);
#else
            localtime_r(&t, &time_info);
#endif
            return time_info.tm_year + 1900;
        }

    } // namespace time

} // namespace util
