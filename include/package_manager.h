#pragma once
#include <string>
#include <spdlog/spdlog.h>
#include <memory>

#include "logger.h"

namespace muuk {
    namespace package_manager {

        extern std::shared_ptr<spdlog::logger> logger_;

        void add_dependency(
            const std::string& toml_path,
            const std::string& repo,
            const std::string& version,
            std::string& git_url,
            const std::string& muuk_path,
            std::string revision,
            const std::string& tag,
            const std::string& branch,
            bool is_system,
            const std::string& target_section
        );

        void download_github_release(const std::string& repo, const std::string& version);

        void install_submodule(const std::string& repo);

        void install(const std::string& toml_path);

        void remove_package(const std::string& package_name, const std::string& toml_path = "muuk.toml", const std::string& lockfile_path = "muuk.lock.toml");
    }
}