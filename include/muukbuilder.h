#ifndef MUUK_BUILDER_H
#define MUUK_BUILDER_H

#include <memory>
#include <string>

#include "buildbackend.hpp"
#include "muukfiler.h"
#include "muuklockgen.h"
#include "rustify.hpp"

class MuukBuilder {
public:
    explicit MuukBuilder(MuukFiler& config_manager);

    Result<void> build(std::string& target_build, const std::string& compiler, const std::string& profile);
    Result<bool> is_compiler_available() const;

private:
    MuukFiler& config_manager_;
    std::unique_ptr<NinjaBackend> ninja_backend_;
    std::unique_ptr<CompileCommandsBackend> compdb_backend_;

    Result<void> execute_build(const std::string& profile) const;
    Result<muuk::Compiler> detect_default_compiler() const;
    std::string detect_archiver(muuk::Compiler compiler) const;
    std::string detect_linker(muuk::Compiler compiler) const;
    Result<std::string> select_profile(const std::string& profile);
    Result<void> add_script(const std::string& profile, const std::string& build_name);

    void generate_compile_commands(const std::string& profile);
};

#endif // MUUK_BUILDER_H
