#pragma once
#ifndef BUILD_BACKEND_H
#define BUILD_BACKEND_H

#include <filesystem>

#include <nlohmann/json.hpp>

#include "build/manager.hpp"
#include "build/targets.hpp"
#include "compiler.hpp"

namespace muuk {
    namespace build {
        class BuildBackend {
        protected:
            const BuildManager& build_manager;
            muuk::Compiler compiler_;
            const std::string archiver_;
            const std::string linker_;

        public:
            virtual ~BuildBackend() = default;

            BuildBackend(
                const BuildManager& build_manager,
                const muuk::Compiler compiler,
                const std::string archiver,
                const std::string linker) :
                build_manager(build_manager),
                compiler_(compiler),
                archiver_(std::move(archiver)),
                linker_(std::move(linker)) {
            }

            virtual void generate_build_file(
                const std::string& profile)
                = 0;
        };

        class NinjaBackend : public BuildBackend {
        private:
            std::filesystem::path build_dir_;

        public:
            NinjaBackend(
                const BuildManager& build_manager,
                const muuk::Compiler compiler,
                const std::string& archiver,
                const std::string& linker);

            void generate_build_file(
                const std::string& profile) override;

        private:
            std::string generate_rule(const CompilationTarget& target);
            std::string generate_rule(const ArchiveTarget& target);
            std::string generate_rule(const LinkTarget& target);
            void generate_rule(const ExternalTarget& target);

            void generate_build_rules(std::ostringstream& out);
            void write_header(std::ostringstream& out, std::string profile);
        };

        class CompileCommandsBackend : public BuildBackend {
        private:
            std::filesystem::path build_dir_;

        public:
            CompileCommandsBackend(
                const BuildManager& build_manager,
                const muuk::Compiler compiler,
                const std::string& archiver,
                const std::string& linker);

            void generate_build_file(
                const std::string& profile) override;

        private:
            nlohmann::json generate_compile_commands(const std::string& profile_cflags);
        };
    } // namespace build
} // namespace muuk

#endif // BUILD_BACKEND_H