#pragma once

#include <string>

#include "buildconfig.h"
#include "rustify.hpp"

namespace muuk {
    Result<void> remove_package(
        const std::string& package_name,
        const std::string& toml_path = "muuk.toml",
        const std::string& lockfile_path = MUUK_CACHE_FILE);
}