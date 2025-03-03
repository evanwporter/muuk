#pragma once

#include "ifileops.hpp"
#include <string>
#include <iostream>

class MockFileOperations : public IFileOperations {
private:
    std::string file_content;

public:
    bool exists() const override {
        return true;
    }

    std::string read_file() const override {
        return file_content;
    }

    void write_file(const std::string& content) override {
        file_content = content;
    }

    std::string get_file_path() const override {
        return "muuk.toml";
    }
};
