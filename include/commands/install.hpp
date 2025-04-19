#pragma once

#include <string>

#include "buildconfig.h"
#include "rustify.hpp"

namespace muuk {

    Result<void> install(
        const std::string& lockfile_path_string = MUUK_CACHE_FILE);
}