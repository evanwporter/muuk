#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <tl/expected.hpp>

#include "buildbackend.hpp"
#include "muukfiler.h"
#include "muuklockgen.h"

class MuukBuilder {
public:
    explicit MuukBuilder(MuukFiler& config_manager);

    tl::expected<void, std::string> build(std::string& target_build, const std::string& compiler, const std::string& profile);
    tl::expected<bool, std::string> is_compiler_available() const;

private:
    MuukFiler& config_manager_;
    std::unique_ptr<NinjaBackend> ninja_backend_;
    std::unique_ptr<CompileCommandsBackend> compdb_backend_;

    std::shared_ptr<spdlog::logger> logger_;

    tl::expected<void, std::string> execute_build(const std::string& profile) const;
    tl::expected<muuk::Compiler, std::string> detect_default_compiler() const;
    std::string detect_archiver(muuk::Compiler compiler) const;
    std::string detect_linker(muuk::Compiler compiler) const;
    tl::expected<std::string, std::string> select_profile(const std::string& profile);
    tl::expected<void, std::string> add_script(const std::string& profile, const std::string& build_name);

    void generate_compile_commands(const std::string& profile);
};

#endif // MUUK_BUILDER_H
