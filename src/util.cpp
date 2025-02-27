#include "muuk.h"
#include "../include/logger.h"
#include "../include/util.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <locale>
#include <spdlog/spdlog.h>
#include <regex>
#include <spdlog/sinks/basic_file_sink.h>
#include <concepts>
#include <array>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <string>

extern "C" {
#include "zip.h"
}

namespace fs = std::filesystem;

namespace util {

    // Ensure directory exists and optionally create .gitignore
    void ensure_directory_exists(const std::string& dir_path, bool gitignore) {
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
            muuk::logger::info("Created directory: {}", dir_path);
        }
        else {
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
                }
                else {
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

    void remove_path(const std::string& path) {
        if (fs::exists(path)) {
            fs::remove_all(path);
            muuk::logger::info("Removed path: {}", path);
        }
        else {
            muuk::logger::warn("Attempted to remove non-existent path: {}", path);
        }
    }

    bool match_pattern(const std::string& path, const std::string& pattern) {
        bool matches = false;
        if (pattern[0] == '!') {
            std::string inner_pattern = pattern.substr(1);
            matches = !std::regex_match(path, std::regex(".*" + std::regex_replace(inner_pattern, std::regex("\\*"), ".*") + "$"));
        }
        else {
            matches = std::regex_match(path, std::regex(".*" + std::regex_replace(pattern, std::regex("\\*"), ".*") + "$"));
        }
        muuk::logger::debug("Matching '{}' with pattern '{}': {}", path, pattern, matches);
        return matches;
    }

    // ZIP Extraction
    void extract_zip(const std::string& archive, const std::string& target_dir) {
        try {
            ensure_directory_exists(target_dir);

            if (!fs::exists(archive)) {
                muuk::logger::error("Zip file does not exist: {}", archive);
                throw std::runtime_error("Zip file not found: " + archive);
            }

            muuk::logger::info("Starting extraction of zip archive: {} from {}", archive, target_dir);

            int result = zip_extract(
                archive.c_str(),
                target_dir.c_str(),
                [](const char* filename, void* arg) -> int {
                    // muuk::logger::info("Extracting: {}", filename);
                    (void)filename;
                    (void)arg;
                    return 0;
                },
                nullptr
            );

            if (result < 0) {
                muuk::logger::error("Failed to extract zip archive '{}'. Error code: {}", archive, result);
                throw std::runtime_error("Zip extraction failed.");
            }

            muuk::logger::info("Extraction completed successfully for: {}", archive);
        }
        catch (const std::exception& ex) {
            muuk::logger::error("Exception during zip extraction: {}", ex.what());
            throw;
        }
        catch (...) {
            muuk::logger::error("Unknown error occurred during zip extraction.");
            throw;
        }
    }

    void download_file(const std::string& url, const std::string& output_path) {
        std::string command;

        if (util::command_exists("wget")) {
            command = "wget --quiet --output-document=" + output_path +
                " --no-check-certificate " + url;
        }
        else if (util::command_exists("curl")) {
            command = "curl -L -o " + output_path + " " + url;
        }
        else {
            spdlog::error("Neither wget nor curl is available on the system.");
            throw std::runtime_error("No suitable downloader found. Install wget or curl.");
        }

        spdlog::info("Executing download command: {}", command);

        int result = util::execute_command(command.c_str());
        if (result != 0) {
            spdlog::error("Failed to download file from {}. Command exited with code: {}", url, result);
            throw std::runtime_error("File download failed.");
        }
    }


    int execute_command(const std::string& command) {
        muuk::logger::info("Executing command: {}", command);
        return system(command.c_str());
    }

    // ==========================
    //  Network Utilities
    // ==========================
    namespace network {

        Result<nlohmann::json> fetch_json(const std::string& url) {
            std::string output_file = "github_api_response.json";
            std::string command = "wget --quiet -O - "
                "--header=\"Accept: application/vnd.github.v3+json\" "
                "--header=\"User-Agent: Mozilla/5.0\" "
                "--no-check-certificate " + url;

            std::string result = util::execute_command_get_out(command);

            try {
                return nlohmann::json::parse(result);
            }
            catch (const std::exception& e) {
                return tl::unexpected("JSON parsing failed: " + std::string(e.what()));
            }
        }
    } // namespace network

#ifdef _WIN32
#include <windows.h>
#endif
#include <set>
#include <vector>

    std::string to_utf8(const std::wstring& wstr) {
#ifdef _WIN32
        if (wstr.empty()) return "";

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8_str(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);
        return utf8_str;
#else
        std::mbstate_t state{};
        const wchar_t* src = wstr.c_str();
        size_t len = 1 + std::wcsrtombs(nullptr, &src, 0, &state);
        std::string dest(len, '\0');
        std::wcsrtombs(&dest[0], &src, len, &state);
        return dest;
#endif
    }


    bool is_valid_utf8(const std::string& str) {
#ifdef _WIN32
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
        return len > 0;
#else
        std::mbstate_t state{};
        const char* src = str.c_str();
        wchar_t tmp;
        return std::mbsrtowcs(&tmp, &src, 1, &state) != static_cast<size_t>(-1);
#endif
    }

    bool command_exists(const std::string& command) {
#ifdef _WIN32
        std::string check_command = "where " + command + " >nul 2>&1";
#else
        std::string check_command = "which " + command + " >/dev/null 2>&1";
#endif
        return (std::system(check_command.c_str()) == 0);
    }

    std::string normalize_path(const std::string& path) {
        try {
            std::filesystem::path fs_path = std::filesystem::absolute(std::filesystem::path(path));
            std::string normalized = fs_path.lexically_normal().string();
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            return normalized;
        }
        catch (const std::exception& e) {
            muuk::logger::warn("Exception during path normalizing {}", e.what());
            return path;
        }
        catch (...) {
            muuk::logger::error("Unknown error occurred during normalize path.");
            throw;
        }
}

    std::vector<std::string> to_linux_path(const std::vector<std::string>& paths, const std::string& prefix) {
        std::vector<std::string> new_paths;
        new_paths.reserve(paths.size());
        for (const auto& path : paths) {
            new_paths.push_back(to_linux_path(path, prefix));
        }
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

#ifdef _WIN32 // Get rid of drive letter
        if (new_path.length() > 2 && new_path[1] == ':') {
            new_path = new_path.substr(0, 1) + "$:" + new_path.substr(2);
        }
#endif

        return (is_absolute ? "" : prefix) + new_path;
    }

    template std::string vectorToString<std::string>(const std::vector<std::string>&, const std::string&);

    std::string execute_command_get_out(const std::string& command) {
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

    std::string join_strings(const std::vector<std::string>& strings, const std::string& delimiter) {
        if (strings.empty()) return "";

        std::ostringstream result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) result << delimiter;
            result << strings[i];
        }
        return result.str();
    }

    // ==========================
    //  Git Utilities
    // ==========================
    namespace git {

        std::string git::get_latest_revision(const std::string& git_url) {
            std::string commit_hash_cmd = "git ls-remote " + git_url + " HEAD";
            std::string output = util::execute_command_get_out(commit_hash_cmd);
            std::string revision = output.substr(0, output.find('\t'));

            if (revision.empty()) {
                muuk::logger::error("Failed to retrieve latest commit hash for '{}'.", git_url);
                throw std::runtime_error("Failed to retrieve latest commit hash.");
            }

            revision.erase(std::remove(revision.begin(), revision.end(), '\n'), revision.end());
            muuk::logger::info("Latest commit hash for {} is {}", git_url, revision);

            return revision;
        }

    } // namespace git

    // ==========================
    //  Time Utilities
    // ==========================
    namespace time {

        int current_year() {
            std::time_t t = std::time(nullptr);
            std::tm time_info{};

#ifdef _WIN32
            localtime_s(&time_info, &t);
#else
            localtime_r(&t, &time_info);
#endif
            return time_info.tm_year + 1900;
        }

    } // namespace time


    } // namespace util


