#pragma once

#include <string>
#include <vector>

class IFileOperations {
public:
    virtual ~IFileOperations() = default;

    virtual bool exists() const = 0;
    virtual std::string read_file() const = 0;
    virtual void write_file(const std::string& content) = 0;
    virtual std::string get_file_path() const = 0;
};
