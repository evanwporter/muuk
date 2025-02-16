#ifndef MUUK_HPP
#define MUUK_HPP

#include <vector>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include "../include/muukbuilder.h"
#include "muukfiler.h"

// // namespace Muuk {
// class IMuukFiler;

class Muuker {
public:
    explicit Muuker(MuukFiler& config_manager);

    void clean() const;
    void run_script(const std::string& script, const std::vector<std::string>& args) const;
    void download_github_release(const std::string& repo_url, const std::string& version = "latest");

    void upload_patch(bool dry_run = false);

    void install_submodule(const std::string& repo);

    void update_muuk_toml_with_submodule(const std::string& author, const std::string& repo_name, const std::string& version);

    void remove_package(const std::string& package_name);

    std::string get_package_name() const;

    void init_project();

    void generate_license(const std::string& license, const std::string& author);

private:
    MuukFiler& config_manager_;
    std::shared_ptr<spdlog::logger> logger_;
    MuukBuilder muuk_builder_;

    void add_dependency(const std::string& author, const std::string& repo_name, const std::string& version);

    void download_patch(const std::string& author, const std::string& repo_name, const std::string& version);
    // void upload_patch(const std::string& author, const std::string& repo_name, const std::string& version);

};
// }

#endif // MUUK_HPP
