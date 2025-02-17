#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include "../include/muukfiler.h"
#include "../include/muuklockgen.h"
#include "../include/ninjagen.h"
#include "../include/logger.h"
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

class MuukBuilder {
public:
    explicit MuukBuilder(MuukFiler& config_manager);

    void build(bool is_release, std::string& target_build, const std::string& compiler);
    bool is_compiler_available() const;

private:
    MuukFiler& config_manager_;
    std::unique_ptr<MuukLockGenerator> lock_generator_;
    std::unique_ptr<NinjaGenerator> ninja_generator_;
    std::shared_ptr<spdlog::logger> logger_;

    void execute_build(bool is_release) const;

    std::string detect_default_compiler() const;
    std::string detect_archiver(const std::string& compiler) const;
    std::string detect_linker(const std::string& compiler) const;
};

#endif // MUUK_BUILDER_H
