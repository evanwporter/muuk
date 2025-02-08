#include "../include/util.h"
#include "../include/logger.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <codecvt>
#include <locale>
#include <spdlog/spdlog.h>
#include <regex>
#include <spdlog/sinks/basic_file_sink.h>
#include <sstream>
#include <concepts>

extern "C" {
#include "zip.h"
}

namespace fs = std::filesystem;

namespace util {

    auto logger = Logger::get_logger("util_logger");

    // Ensure directory exists and optionally create .gitignore
    void ensure_directory_exists(const std::string& dir_path, bool gitignore) {
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
            logger->info("Created directory: {}", dir_path);
        }
        else {
            logger->debug("Directory already exists: {}", dir_path);
        }

        if (gitignore) {
            std::string gitignore_file = dir_path + "/.gitignore";
            if (!fs::exists(gitignore_file)) {
                std::ofstream gitignore_stream(gitignore_file);
                if (gitignore_stream.is_open()) {
                    gitignore_stream << "*\n";
                    gitignore_stream.close();
                    logger->info("Created .gitignore file in directory: {}", dir_path);
                }
                else {
                    logger->error("Failed to create .gitignore file in directory: {}", dir_path);
                }
            }
        }
    }

    bool path_exists(const std::string& path) {
        bool exists = fs::exists(path);
        logger->debug("Checked existence of '{}': {}", path, exists);
        return exists;
    }

    void remove_path(const std::string& path) {
        if (fs::exists(path)) {
            fs::remove_all(path);
            logger->info("Removed path: {}", path);
        }
        else {
            logger->warn("Attempted to remove non-existent path: {}", path);
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
        logger->debug("Matching '{}' with pattern '{}': {}", path, pattern, matches);
        return matches;
    }

    // ZIP Extraction
    void extract_zip(const std::string& archive, const std::string& target_dir) {
        try {
            ensure_directory_exists(target_dir);

            if (!fs::exists(archive)) {
                logger->error("[util::extract_zip] Zip file does not exist: {}", archive);
                throw std::runtime_error("Zip file not found: " + archive);
            }

            logger->info("[util::extract_zip] Starting extraction of zip archive: {} from {}", archive, target_dir);

            int result = zip_extract(
                archive.c_str(),
                target_dir.c_str(),
                [](const char* filename, void* arg) -> int {
                    // logger->info("Extracting: {}", filename);
                    return 0;
                },
                nullptr
            );

            if (result < 0) {
                logger->error("[util::extract_zip] Failed to extract zip archive '{}'. Error code: {}", archive, result);
                throw std::runtime_error("Zip extraction failed.");
            }

            logger->info("[util::extract_zip] Extraction completed successfully for: {}", archive);
        }
        catch (const std::exception& ex) {
            logger->error("[util::extract_zip] Exception during zip extraction: {}", ex.what());
            throw;
        }
        catch (...) {
            logger->error("[util::extract_zip] Unknown error occurred during zip extraction.");
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
            spdlog::error("[util::download_file] Neither wget nor curl is available on the system.");
            throw std::runtime_error("No suitable downloader found. Install wget or curl.");
        }

        spdlog::info("[util::download_file] Executing download command: {}", command);

        int result = util::execute_command(command.c_str());
        if (result != 0) {
            spdlog::error("[util::download_file] Failed to download file from {}. Command exited with code: {}", url, result);
            throw std::runtime_error("File download failed.");
        }
    }


    int execute_command(const std::string& command) {
        logger->info("[util::execute_command] Executing command: {}", command);
        return system(command.c_str());
    }

    nlohmann::json fetch_json(const std::string& url) {
        std::string output_file = "github_api_response.json";
        std::string command = "wget --quiet --output-document=" + output_file +
            " --header=\"Accept: application/vnd.github.v3+json\" "
            "--header=\"User-Agent: Mozilla/5.0\" "
            "--no-check-certificate " + url;

        spdlog::info("[util::fetch_json] Executing wget command: {}", command);

        int result = util::execute_command(command.c_str());
        if (result != 0) {
            spdlog::error("[util::fetch_json] wget failed with error code: {}", result);
            throw std::runtime_error("Failed to fetch JSON from GitHub API.");
        }

        // Read the JSON file
        std::ifstream json_file(output_file);
        if (!json_file.is_open()) {
            spdlog::error("[util::fetch_json] Failed to open the JSON response file: {}", output_file);
            throw std::runtime_error("Failed to open the fetched JSON file.");
        }

        std::stringstream buffer;
        buffer << json_file.rdbuf();
        json_file.close();

        // Parse and return JSON data
        return nlohmann::json::parse(buffer.str());

        if (std::filesystem::exists(output_file)) {
            std::filesystem::remove(output_file);
            spdlog::info("[util::fetch_json] Deleted temporary JSON response file: {}", output_file);
        }
    }

    std::string to_utf8(const std::wstring& wstr) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }

    bool is_valid_utf8(const std::string& str) {
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring wide_str = converter.from_bytes(str);
            std::string back_to_utf8 = converter.to_bytes(wide_str);
            return back_to_utf8 == str;
        }
        catch (const std::exception&) {
            return false;  // Invalid UTF-8 detected
        }
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
            return path; // Fallback if an error occurs
        }
    }

    std::string to_linux_path(const std::string& path) {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized;
    }

    // Normalize a vector of flags
    std::string normalize_flags(const std::vector<std::string>& flags) {
        std::string normalized;
        for (const auto& flag : flags) {
            normalized += " " + normalize_flag(flag);
        }
        return normalized;
    }

    std::string normalize_flag(const std::string& flag) {
        static const std::unordered_map<std::string, std::string> msvc_to_gcc = {
            {"/I", "-I"}, {"/Fe", "-o"}, {"/Fo", "-o"},
            {"/c", "-c"}, {"/W0", "-w"}, {"/W1", "-Wall"}, {"/W2", "-Wall -Wextra"},
            {"/W3", "-Wall -Wextra -Wpedantic"}, {"/W4", "-Wall -Wextra -Wpedantic -Wconversion"},
            {"/EHsc", "-fexceptions"}, {"/Zi", "-g"}, {"/O2", "-O2"},
            {"/O3", "-O3"}, {"/GL", "-flto"}, {"/link", "-Wl,"}, {"/utf-8", "-finput-charset=UTF-8"}
        };

        static const std::unordered_map<std::string, std::string> gcc_to_msvc = {
            {"-I", "/I"}, {"-o", "/Fe"}, {"-c", "/c"},
            {"-w", "/W0"}, {"-Wall", "/W3"}, {"-Wextra", "/W4"},
            {"-Wpedantic", "/W4"}, {"-Wconversion", "/W4"},
            {"-fexceptions", "/EHsc"}, {"-g", "/Zi"}, {"-O2", "/O2"},
            {"-O3", "/O3"}, {"-flto", "/GL"}, {"-Wl,", "/link"},
            {"-finput-charset=UTF-8", "/utf-8"}
        };

        static const std::regex std_pattern(R"((?:\/std:c\+\+|-std=c\+\+)(\d+))");

        std::smatch match;

        std::string normalized_flag = flag;

        if (flag.starts_with("/D")) {
            return "-D" + flag.substr(2);
        }
        else if (flag.starts_with("-D")) {
            return "/D" + flag.substr(2);
        }

        if (flag[0] != '/' && flag[0] != '-') {
#ifdef _WIN32
            normalized_flag = "/" + flag;
#else
            normalized_flag = "-" + flag;
#endif
        }

#ifdef _WIN32
        auto lookup = gcc_to_msvc.find(normalized_flag);
        if (lookup != gcc_to_msvc.end()) {
            return lookup->second;
        }
#else
        auto lookup = msvc_to_gcc.find(normalized_flag);
        if (lookup != msvc_to_gcc.end()) {
            return lookup->second;
        }
#endif

        // Handle C++ standard flag conversion
        if (std::regex_match(flag, match, std_pattern)) {
#ifdef _WIN32
            return "/std:c++" + match[1].str();
#else
            return "-std=c++" + match[1].str();
#endif
        }

        return normalized_flag;
    }

    template std::string vectorToString<std::string>(const std::vector<std::string>&, const std::string&);

} // namespace util

