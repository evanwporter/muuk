#ifndef MUUKBUILDER_H
#define MUUKBUILDER_H

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "../include/muukfiler.h"
#include "../include/muuklockgen.h"
#include "../include/ninjagen.h"


namespace fs = std::filesystem;

class MuukBuilder {
public:
    explicit MuukBuilder(IMuukFiler& config_manager);

    void build(const std::vector<std::string>& args, bool is_release);

private:
    IMuukFiler& config_manager_;
    std::shared_ptr<spdlog::logger> logger_;
    NinjaGenerator ninja_generator_;
    MuukLockGenerator lock_generator_;

    void execute_build(bool is_release) const;

    bool is_compiler_available() const;

};

#endif // MUUKBUILDER_H
