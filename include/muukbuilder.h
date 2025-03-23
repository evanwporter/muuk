#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include <string>

#include "muukfiler.h"
#include "rustify.hpp"

namespace muuk {
    Result<void> build(
        std::string& target_build,
        const std::string& compiler,
        const std::string& profile,
        MuukFiler& config);
}

#endif // MUUK_BUILDER_H
