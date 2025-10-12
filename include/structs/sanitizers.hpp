#pragma once

#include <toml.hpp>

namespace muuk {

    struct Sanitizers {
        bool address = false; // ASan
        bool thread = false; // TSan
        bool undefined = false; // UBSan
        bool memory = false; // MSan
        bool leak = false; // LSan
    };

    void load(Sanitizers& sanitizers, const toml::value& v);

    void serialize(const Sanitizers& sanitizers, toml::value& out);

}