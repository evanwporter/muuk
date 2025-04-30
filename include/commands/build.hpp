#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include <string>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> build(
        const std::string& target_build,
        const std::string& compiler,
        const std::string& profile,
        const toml::value& config,
        const std::string& jobs);
}

#endif // MUUK_BUILDER_H
