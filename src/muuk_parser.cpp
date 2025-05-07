#include <filesystem>
#include <string>
#include <vector>

#include <toml.hpp>

#include "logger.hpp"
#include "muuk_parser.hpp"
#include "muukvalidator.hpp"
#include "rustify.hpp"
#include "toml_ext.hpp"

namespace fs = std::filesystem;

namespace muuk {

    Result<toml::value> parse_muuk_file(const std::string& path, bool is_lockfile) {

        if (!fs::exists(path))
            return make_error<EC::MuukTOMLNotFound>(path);

        muuk::logger::info("Parsing muuk file: {}", path);

        auto parsed_result = toml::try_parse<toml::ordered_type_config>(path);

        if (parsed_result.is_err()) {
            const auto& errors = parsed_result.unwrap_err();
            return Err(toml::format_error("TOML parse error", errors.front()));
        }

        toml::value parsed = parsed_result.unwrap();
        if (!parsed.is_table())
            // If you see this message then something went horribly wrong
            return Err("Root of '{}' must be a TOML table.", path);

        // Validate against schema
        if (!is_lockfile)
            TRYV(validation::validate_muuk_toml(parsed));

        muuk::logger::info("Successfully parsed and validated '{}'", path);
        return parsed;
    }

    std::vector<std::string> parse_array_as_vec(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix) {
        std::vector<std::string> result;

        if (!table.contains(key))
            return result;

        auto raw_vec = toml::try_get<std::vector<std::string>>(table.at(key));
        if (!raw_vec)
            muuk::logger::warn(raw_vec);
        else {
            for (const auto& item : raw_vec.value())
                result.push_back(prefix + item);
        }
        return result;
    }
} // namespace muuk
