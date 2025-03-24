#ifndef MUUK_HPP
#define MUUK_HPP

#include "muukfiler.h"
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace muuk {
    void clean(MuukFiler& config_manager);
    void run_script(
        const MuukFiler& config_manager,
        const std::string& script,
        const std::vector<std::string>& args);
}

#endif // MUUK_HPP
