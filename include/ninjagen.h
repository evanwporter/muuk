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
    NinjaGenerator(const std::string& lockfile_path, const std::string& build_type);
    void generate_ninja_file();

private:
    std::string lockfile_path_;
    std::string build_type_;
    std::string compiler_;
    std::string archiver_;
    std::string ninja_file_;
    fs::path build_dir_;
    std::unique_ptr<MuukFiler> muuk_filer_;
    toml::table config_;
    std::shared_ptr<spdlog::logger> logger_;  // Logger instance

    void write_ninja_header(std::ofstream& out);
    std::pair<std::map<std::string, std::vector<std::string>>, std::vector<std::string>> compile_objects(std::ofstream& out);
    void archive_libraries(std::ofstream& out, const std::map<std::string, std::vector<std::string>>& objects, std::vector<std::string>& libraries);
    void link_executable(std::ofstream& out, const std::map<std::string, std::vector<std::string>>& objects, const std::vector<std::string>& libraries, const std::string& build_name);
};

#endif // NINJA_GENERATOR_H
