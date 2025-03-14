#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>

#include "ifileops.hpp"

class FileOperations : public IFileOperations {
private:
    std::string file_path;

public:
    explicit FileOperations(const std::string& path) :
        file_path(path) { }

    bool exists() const override {
        return std::filesystem::exists(file_path);
    }

    std::string read_file() const override {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void write_file(const std::string& content) override {
        std::ofstream file(file_path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + file_path);
        }
        file << content;
    }

    std::string get_file_path() const override {
        return file_path;
    }
};
