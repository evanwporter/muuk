#include <string>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

#include "lockgen/config/base.hpp"
#include "logger.hpp"
#include "toml_ext.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {
    namespace lockgen {
        std::vector<source_file> parse_sources(const toml::value& section, const std::string& base_path, const std::string& key) {
            std::vector<source_file> temp_sources;
            if (!section.contains(key))
                return temp_sources;

            for (const auto& src : section.at(key).as_array()) {

                // (1)
                // If its a string parse the path and flags out of it
                // Parsing flags out of the path may get deprecated in the future
                // since its more complicated
                if (src.is_string()) {
                    std::string source_entry = src.as_string();

                    std::unordered_set<std::string> extracted_cflags;
                    std::string file_path;

                    size_t space_pos = source_entry.find(' ');
                    if (space_pos != std::string::npos) {
                        file_path = source_entry.substr(0, space_pos);
                        std::string flags_str = source_entry.substr(space_pos + 1);

                        std::istringstream flag_stream(flags_str);
                        std::string flag;
                        while (flag_stream >> flag)
                            extracted_cflags.insert(flag);
                    } else {
                        file_path = source_entry;
                    }

                    std::filesystem::path full_path = std::filesystem::path(file_path);
                    if (!full_path.is_absolute())
                        full_path = std::filesystem::path(base_path) / full_path;

                    temp_sources.emplace_back(
                        util::file_system::to_linux_path(full_path.lexically_normal().string()),
                        extracted_cflags);
                }

                // (2)
                // If its a table parse the path and flags out of it
                else if (src.is_table()) {
                    auto src_table = src.as_table();

                    // This should get caught by the validator, but just in case
                    if (!src_table.contains("path")) {
                        muuk::logger::warn("Source entry is missing 'path' key.");
                        continue;
                    }
                    const fs::path path = src_table.at("path").as_string();
                    const auto cflags = toml::try_find_or<std::unordered_set<std::string>>(
                        src,
                        "cflags",
                        {});

                    temp_sources.emplace_back(
                        util::file_system::to_linux_path(path.lexically_normal().string()),
                        cflags);
                }
            }

            return temp_sources;
        }

        std::unordered_set<std::string> parse_libs(const toml::value& section, const std::string& base_path) {
            std::unordered_set<std::string> resolved_libs;

            auto raw_libs = toml::try_find_or<std::unordered_set<std::string>>(section, "libs", {});
            for (const auto& lib : raw_libs) {
                std::filesystem::path lib_path(lib);
                if (!lib_path.is_absolute())
                    lib_path = std::filesystem::path(base_path) / lib_path;
                resolved_libs.insert(util::file_system::to_linux_path(lib_path.lexically_normal().string()));
            }
            return resolved_libs;
        }

        std::vector<source_file> expand_glob_sources(const std::vector<source_file>& input_sources) {
            std::vector<source_file> expanded;

            for (const auto& s : input_sources) {
                try {
                    std::vector<std::filesystem::path> globbed_paths = glob::glob(s.path);
                    for (const auto& path : globbed_paths)
                        expanded.emplace_back(util::file_system::to_linux_path(path.string()), s.cflags);

                } catch (const std::exception& e) {
                    muuk::logger::warn("Error while globbing '{}': {}", s.path, e.what());
                }
            }

            return expanded;
        }
    } // namespace lockgen
} // namespace muuk