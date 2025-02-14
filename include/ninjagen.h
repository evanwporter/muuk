#ifndef NINJA_GENERATOR_H
#define NINJA_GENERATOR_H

#include "../include/muukfiler.h"
#include "../include/logger.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

class NinjaGenerator {
public:
    NinjaGenerator(const std::string& lockfile_path, const std::string& build_type, const std::string& compiler, const std::string& archiver, const std::string& linker);
    void generate_ninja_file(const std::string& target_build);

private:
    std::string lockfile_path_;
    std::string build_type_;

    std::string compiler_;
    std::string archiver_;
    std::string linker_;

    std::string ninja_file_;
    fs::path build_dir_;
    std::unique_ptr<MuukFiler> muuk_filer_;
    toml::table config_;
    std::shared_ptr<spdlog::logger> logger_;

    void write_ninja_header(std::ofstream& out);
    std::pair<std::unordered_map<std::string, std::vector<std::string>>, std::vector<std::string>> compile_objects(std::ofstream& out, const std::unordered_map<std::string, std::vector<std::string>>& dependencies_map, const std::unordered_map<std::string, std::vector<std::string>>& modules_map);
    // std::pair<std::unordered_map<std::string, std::vector<std::string>>, std::vector<std::string>> compile_objects(std::ofstream& out);
    void archive_libraries(std::ofstream& out, const std::unordered_map<std::string, std::vector<std::string>>& objects, std::vector<std::string>& libraries);
    void link_executable(std::ofstream& out, const std::unordered_map<std::string, std::vector<std::string>>& objects, const std::vector<std::string>& libraries, const std::string& build_name);

    std::string gather_cflags(const toml::table& pkg_table);
};

#endif // NINJA_GENERATOR_H
