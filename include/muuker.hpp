#ifndef MUUK_HPP
#define MUUK_HPP

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace muuk {
    void clean();
    void run_script(
        // const MuukFiler& config_manager,
        const std::string& script,
        const std::vector<std::string>& args);
}

#endif // MUUK_HPP
