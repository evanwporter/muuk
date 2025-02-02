#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include "../include/muukfiler.h"
#include "../include/muuklockgen.h"
#include "../include/ninjagen.h"
#include "../include/util.h"
#include "../include/logger.h"
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

class MuukBuilder {
public:
    explicit MuukBuilder(IMuukFiler& config_manager);

    void build(const std::vector<std::string>& args, bool is_release);
    bool is_compiler_available() const;

private:
    IMuukFiler& config_manager_;
    MuukLockGenerator lock_generator_;
    NinjaGenerator ninja_generator_;
    std::shared_ptr<spdlog::logger> logger_;

    void execute_build(bool is_release) const;
};

#endif // MUUK_BUILDER_H
