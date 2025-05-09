#include <string>
#include <vector>

#include <toml.hpp>

#include "logger.hpp"
#include "muuk_parser.hpp"
#include "toml11/types.hpp"
#include "toml_ext.hpp"

namespace muuk {

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
