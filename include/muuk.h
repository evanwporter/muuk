#ifndef MUUK_HPP
#define MUUK_HPP

#include <vector>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include "../include/muukbuilder.h"

class IMuukFiler;

class Muuk {
public:
    explicit Muuk(IMuukFiler& config_manager);

    void clean() const;
    void run_script(const std::string& script, const std::vector<std::string>& args) const;
    void build(const std::vector<std::string>& args) const;
    void download_github_release(const std::string& repo_url, const std::string& version = "latest");

    void upload_patch(bool dry_run = false);

private:
    IMuukFiler& config_manager_;
    std::shared_ptr<spdlog::logger> logger_;
    MuukBuilder muuk_builder_;

    void add_dependency(const std::string& author, const std::string& repo_name, const std::string& version);

    void download_patch(const std::string& author, const std::string& repo_name, const std::string& version);
    // void upload_patch(const std::string& author, const std::string& repo_name, const std::string& version);

};

#endif // MUUK_HPP
