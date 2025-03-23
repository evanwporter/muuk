#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <tl/expected.hpp>

#include "buildconfig.h"
#include "rustify.hpp"

namespace muuk {
    namespace package_manager {

        extern std::shared_ptr<spdlog::logger> logger_;

        tl::expected<void, std::string> add_dependency(
            const std::string& toml_path,
            const std::string& repo,
            std::string version,
            std::string& git_url,
            std::string& muuk_path,
            bool is_system,
            const std::string& target_section);

        tl::expected<void, std::string> install(
            const std::string& lockfile_path_string = MUUK_CACHE_FILE);

        Result<void> remove_package(
            const std::string& package_name,
            const std::string& toml_path = "muuk.toml",
            const std::string& lockfile_path = MUUK_CACHE_FILE);
    }
}