#pragma once
#ifndef MUUK_PARSER_HPP
#define MUUK_PARSER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include <toml.hpp>

#include "logger.hpp"
#include "muukvalidator.hpp"
#include "rustify.hpp"
#include "toml_ext.hpp"

namespace muuk {

    template <typename TypeConfig = toml::ordered>
    [[nodiscard]] Result<toml::basic_value<TypeConfig>>
    parse_muuk_file(const std::string& path, bool is_lockfile = false) {
        namespace fs = std::filesystem;

        if (!fs::exists(path))
            return make_error<EC::MuukTOMLNotFound>(path);

        muuk::logger::info("Parsing muuk file: {}", path);

        auto parsed_result = toml::try_parse<TypeConfig>(path);

        if (parsed_result.is_err()) {
            const auto& errors = parsed_result.unwrap_err();
            return Err(toml::format_error("TOML parse error", errors.front()));
        }

        auto parsed = parsed_result.unwrap();
        if (!parsed.is_table())
            // If you see this message then something went horribly wrong
            return Err("Root of '{}' must be a TOML table.", path);

        // Validate against schema
        if (!is_lockfile)
            TRYV(validation::validate_muuk_toml(parsed));

        muuk::logger::info("Successfully parsed and validated '{}'", path);
        return parsed;
    }

    inline Result<toml::basic_value<toml::ordered_type_config>>
    parse_ordered_muuk_file(const std::string& path, bool is_lockfile = false) {
        return parse_muuk_file<toml::type_config>(path, is_lockfile);
    }

    std::vector<std::string> parse_array_as_vec(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix = "");

} // namespace muuk

#endif // MUUK_PARSER_HPP