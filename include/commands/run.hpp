#include <string>
#include <vector>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> run_script(
        const toml::value config,
        const std::string& script,
        const std::vector<std::string>& args);
}