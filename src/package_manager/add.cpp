#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
#include <toml.hpp>

#include "buildconfig.h"
#include "commands/add.hpp"
#include "logger.hpp"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "toml11/types.hpp"
#include "util.hpp"

#ifdef SEARCH_FOR_MUUK_PATCHES
constexpr const char* MUUK_PATCH_UTL = "https://raw.githubusercontent.com/evanwporter/muuk/main/muuk-patches/";
#endif

namespace muuk {

    std::pair<std::string, std::string> split_author_repo(std::string repo) {
        size_t slash_pos = repo.find('/');
        if (slash_pos == std::string::npos) {
            muuk::logger::error("Invalid repository format. Expected <author>/<repo> but got: {}", repo);
            return { "", "" };
        }

        std::string author = repo.substr(0, slash_pos);
        std::string repo_name = repo.substr(slash_pos + 1);

        if (repo_name.empty()) {
            muuk::logger::error(
                "Invalid repository format. Expected <author>/<repo> but got: {}",
                repo);
            return { "", "" };
        }

        return { author, repo_name };
    }

    Result<void> add(const std::string& toml_path, const std::string& repo, std::string& version, std::string& git_url, std::string& muuk_path, bool is_system, const std::string& target_section) {
        (void)target_section; // TODO: Use target_section
        muuk::logger::info(
            "Adding dependency to '{}': {} (version: {})",
            toml_path,
            repo,
            version);

        try {
            auto parsed = muuk::parse_muuk_file<toml::ordered_type_config>(toml_path);
            if (!parsed)
                return Err(parsed);
            auto root = parsed.value();

            // Create the dependencies table if it doesn't exist
            if (!root.contains("dependencies") || !root.at("dependencies").is_table()) {
                muuk::logger::info("Creating 'dependencies' section in the TOML file.");
                root["dependencies"] = toml::table();
            }
            auto& dependencies = root.at("dependencies").as_table();

            auto [author, repo_name] = split_author_repo(repo);
            if (repo_name.empty())
                return Err(
                    "Invalid repository format. Expected <author>/<repo>");

            if (dependencies.contains(repo_name))
                return Err(
                    "Dependency '{}' already exists in '{}'.",
                    repo_name,
                    toml_path);

            // Determine Git URL and version
            if (!is_system) {
                if (git_url.empty())
                    git_url = "https://github.com/" + repo + ".git";

                muuk::logger::info("No tag, version, or revision provided. Fetching latest commit hash...");
                if (version.empty())
                    version = util::git::get_latest_revision(git_url);
            }

            const std::string final_git_url = git_url.empty()
                ? "https://github.com/" + author + "/" + repo_name + ".git"
                : git_url;
            const std::string target_dir = std::string(DEPENDENCY_FOLDER) + "/" + repo_name + "/" + version;

            // Ensure dependency folder exists
            util::file_system::ensure_directory_exists(
                DEPENDENCY_FOLDER,
                true);
            util::file_system::ensure_directory_exists(
                target_dir);

            if (muuk_path.empty()) {
                muuk_path = target_dir + "/muuk.toml";
                const std::string muuk_toml_url = "https://raw.githubusercontent.com/" + author + "/" + repo_name + "/" + version + "/muuk.toml";

                if (!util::network::download_file(muuk_toml_url, muuk_path)) {
#ifdef SEARCH_FOR_MUUK_PATCHES
                    muuk::logger::warn("Failed to download muuk.toml from repo, searching for patches...");
                    std::string patch_muuk_toml_url = MUUK_PATCH_UTL + repo_name + "/muuk.toml";
                    if (!util::network::download_file(patch_muuk_toml_url, muuk_path)) {
                        muuk::logger::warn("No valid patch found. Generating a default `muuk.toml`.");
                        if (!qinit_library(author, repo_name, version))
                            return Err("Failed to generate default muuk.toml.");
                    }
#else
                    muuk::logger::warn("Failed to download muuk.toml from repo. Generating a default `muuk.toml`.");
#endif
                }
            }

            dependencies[repo_name] = toml::value(version);

            std::ofstream file_out(toml_path, std::ios::trunc);
            if (!file_out.is_open())
                return Err(
                    "Failed to open TOML file for writing: {}",
                    toml_path);
            file_out << toml::format(root);
            file_out.close();

            muuk::logger::info(
                "Added dependency '{}' to '{}'",
                repo_name,
                toml_path);
            return {};

        } catch (const std::exception& e) {
            return Err("Error adding dependency: {}", std::string(e.what()));
        }
    }
}
