#ifndef NINJAGEN_H
#define NINJAGEN_H

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include "toml.hpp"

class NinjaGenerator {
public:
    explicit NinjaGenerator();
    void GenerateNinjaFile(const std::string& lockfile_path) const;

private:
    std::shared_ptr<spdlog::logger> logger_;

    // Helper functions
    static std::string normalize_path(const std::string& path);
    static std::vector<std::string> get_list(const toml::table& tbl, const std::string& key);
    static std::string join(const std::vector<std::string>& vec, const std::string& sep = " ");
};

#endif // NINJAGEN_H
