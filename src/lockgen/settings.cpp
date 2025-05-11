#include <string>
#include <toml.hpp>

#include "lockgen/config/base.hpp"
#include "opt_level.hpp"

namespace muuk {
    namespace lockgen {
        void load(Settings& settings, const toml::value& v) {
            if (v.contains("opt-level")) {
                // Can be a string
                if (v.at("opt-level").is_string()) {
                    settings.optimization_level = opt_lvl_from_string(
                        v.at("opt-level").as_string());
                }

                // Or an integer
                else if (v.at("opt-level").is_integer()) {
                    settings.optimization_level = opt_lvl_from_string(
                        std::to_string(v.at("opt-level").as_integer()));
                }
            }

            if (v.contains("lto"))
                settings.lto = v.at("lto").as_boolean();
            if (v.contains("debug"))
                settings.debug = v.at("debug").as_boolean();
            if (v.contains("rpath"))
                settings.rpath = v.at("rpath").as_boolean();
            if (v.contains("debug-assertions"))
                settings.debug_assertions = v.at("debug-assertions").as_boolean();
        }

        void serialize(const Settings& settings, toml::value& out) {
            out["opt-level"] = to_string(settings.optimization_level);
            out["lto"] = settings.lto;
            out["debug"] = settings.debug;
            out["rpath"] = settings.rpath;
            out["debug-assertions"] = settings.debug_assertions;
        }

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
    };
}