#pragma once

#include "logger.h"
#include "rustify.hpp"

#include <tl/expected.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <memory>

namespace muuk {
    namespace package_manager {

        extern std::shared_ptr<spdlog::logger> logger_;

        tl::expected<void, std::string> add_dependency(
            const std::string& toml_path,
            const std::string& repo,
            const std::string& version,
            std::string& git_url,
            std::string& muuk_path,
            std::string revision,
            const std::string& tag,
            const std::string& branch,
            bool is_system,
            const std::string& target_section
        );

        tl::expected<void, std::string> install(
            const std::string& lockfile_path_string = "muuk.lock.toml"
        );

        Result<void> remove_package(
            const std::string& package_name,
            const std::string& toml_path = "muuk.toml",
            const std::string& lockfile_path = "muuk.lock.toml"
        );
    }
}