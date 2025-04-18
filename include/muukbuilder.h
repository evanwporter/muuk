#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include <string>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> build(
        std::string& target_build,
        const std::string& compiler,
        const std::string& profile,
        const toml::value& config);
}

#endif // MUUK_BUILDER_H
