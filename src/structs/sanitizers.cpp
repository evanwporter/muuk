#include <string>

#include <toml.hpp>

#include "logger.hpp"
#include "structs/sanitizers.hpp"

namespace muuk {
    void load(Sanitizers& sanitizers, const toml::value& v) {
        if (!v.contains("sanitizers"))
            return;
        for (const auto& item : v.at("sanitizers").as_array()) {
            if (!item.is_string())
                continue;

            const std::string name = item.as_string();

            if (name == "address") {
                sanitizers.address = true;
            } else if (name == "thread") {
                sanitizers.thread = true;
            } else if (name == "undefined") {
                sanitizers.undefined = true;
            } else if (name == "memory") {
                sanitizers.memory = true;
            } else if (name == "leak") {
                sanitizers.leak = true;
            } else {
                muuk::logger::warn("Unknown sanitizer '{}'", name);
            }
        }
    }

    void serialize(const Sanitizers& sanitizers, toml::value& out) {
        toml::array san_array;

        if (sanitizers.address)
            san_array.push_back("address");
        if (sanitizers.thread)
            san_array.push_back("thread");
        if (sanitizers.undefined)
            san_array.push_back("undefined");
        if (sanitizers.memory)
            san_array.push_back("memory");
        if (sanitizers.leak)
            san_array.push_back("leak");

        if (!san_array.empty())
            out["sanitizers"] = std::move(san_array);
    }
} // namespace muuk