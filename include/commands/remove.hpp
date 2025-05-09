#pragma once

#include <string>

#include "buildconfig.h"
#include "rustify.hpp"

namespace muuk {
    Result<void> remove_package(
        const std::string& package_name,
        const std::string& toml_path = MUUK_TOML_FILE);
}