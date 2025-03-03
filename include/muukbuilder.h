#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include "../include/muukfiler.h"
#include "../include/muuklockgen.h"
#include "../include/ninjabackend.hpp"  
#include "../include/logger.h"

#include "muuk.h"

#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

class MuukBuilder {
public:
    explicit MuukBuilder(MuukFiler& config_manager);

    void build(std::string& target_build, const std::string& compiler, const std::string& profile);
    bool is_compiler_available() const;

private:
    MuukFiler& config_manager_;
    std::unique_ptr<MuukLockGenerator> lock_generator_;
    std::unique_ptr<NinjaBackend> ninja_backend_;
    std::shared_ptr<spdlog::logger> logger_;

    void execute_build(const std::string& profile) const;
    muuk::compiler::Compiler detect_default_compiler() const;
    std::string detect_archiver(muuk::compiler::Compiler compiler) const;
    std::string detect_linker(muuk::compiler::Compiler compiler) const;
    void add_script(const std::string& profile);
};

#endif // MUUK_BUILDER_H
