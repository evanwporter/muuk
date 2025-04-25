#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include <toml.hpp>

#include "error_codes.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "muukvalidator.hpp"
#include "rustify.hpp"

namespace muuk {

    Result<toml::value> parse_muuk_file(const std::string& path, bool is_lockfile, bool preserve_order) {
        namespace fs = std::filesystem;

        if (!fs::exists(path)) {
            return make_error<EC::FileNotFound>(path);
        }

        try {
            muuk::logger::info("Parsing muuk file: {}", path);

            toml::value parsed;
            if (preserve_order)
                parsed = toml::parse<toml::ordered_type_config>(path);
            else
                parsed = toml::parse(path);

            if (!parsed.is_table()) {
                return Err("Root of '{}' must be a TOML table.", path);
            }

            // Validate against schema
            if (!is_lockfile) {
                auto validation = validate_muuk_toml(parsed);
                if (!validation) {
                    return Err(validation.error());
                }
            }

            muuk::logger::info("Successfully parsed and validated '{}'", path);
            return parsed;

        } catch (const toml::syntax_error& err) {
            return Err("TOML syntax error in '{}': {}", path, err.what());
        } catch (const std::exception& ex) {
            return Err("Failed to parse '{}': {}", path, ex.what());
        }
    }

    std::vector<std::string> parse_array_as_vec(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix) {
        std::vector<std::string> result;

        if (!table.contains(key))
            return result;

        try {
            auto raw_vec = toml::get<std::vector<std::string>>(table.at(key));
            for (const auto& item : raw_vec) {
                result.push_back(prefix + item);
            }
        } catch (const std::exception& e) {
            muuk::logger::warn("Failed to parse array '{}' as vector<string>: {}", key, e.what());
        }

        return result;
    }

    std::unordered_set<std::string> parse_array_as_set(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix) {
        std::unordered_set<std::string> result;

        if (!table.contains(key))
            return result;

        try {
            auto raw_vec = toml::get<std::vector<std::string>>(table.at(key));
            for (const auto& item : raw_vec) {
                result.insert(prefix + item);
            }
        } catch (const std::exception& e) {
            muuk::logger::warn("Failed to parse array '{}' as set<string>: {}", key, e.what());
        }

        return result;
    }

} // namespace muuk
