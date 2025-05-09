#pragma once

#include <string>

#include <rustify.hpp>

namespace muuk {
    Result<void> add( // Hopefully there will be no conflcts in the future
        const std::string& toml_path,
        const std::string& repo,
        std::string& version,
        std::string& git_url,
        std::string& muuk_path,
        bool is_system,
        const std::string& target_section);
}