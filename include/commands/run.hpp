#include <string>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> run_build_target(std::string& target_name, const std::string& profile);
}