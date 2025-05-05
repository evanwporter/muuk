#pragma once
#include <string>
#include <vector>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {

    Result<toml::value> parse_muuk_file(const std::string& path, bool is_lockfile = false);

    std::vector<std::string> parse_array_as_vec(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix = "");

} // namespace muuk
