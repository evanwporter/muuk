// Please note that the test code for each module and the compilation
// rules for each target are taken and adapted from the Clang documentation
// located at:
// https://clang.llvm.org/docs/StandardCPlusPlusModules.html#discovering-dependencies
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "buildmanager.h"
#include "buildtargets.h"
#include "logger.h"
#include "moduleresolver.hpp"

namespace fs = std::filesystem;

class ModuleResolutionTest : public ::testing::Test {
protected:
    fs::path temp_dir;
    BuildManager manager;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "module_cpp_test";
        fs::create_directories(temp_dir);
        muuk::logger::info("Temp test dir: {}", temp_dir.string());

        write_source_files();
        populate_build_targets(manager);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_source_files() {
        std::vector<std::pair<std::string, std::string>> files = {
            { "M.cppm", R"(export module M;
export import :interface_part;
import :impl_part;
export int Hello();)" },
            { "interface_part.cppm", R"(export module M:interface_part;
export void World();)" },
            { "Impl.cpp", R"(module;
#include <iostream>
module M;
void Hello() {
    std::cout << "Hello ";
})" },
            { "impl_part.cppm", R"(module;
#include <string>
#include <iostream>
module M:impl_part;
import :interface_part;

std::string W = "World.";
void World() {
    std::cout << W << std::endl;
})" },
            { "User.cpp", R"(import M;
import third_party_module;
int main() {
  Hello();
  World();
  return 0;
})" }
        };

        for (const auto& [filename, content] : files) {
            std::ofstream ofs(temp_dir / filename);
            ASSERT_TRUE(ofs.is_open()) << "Failed to write: " << filename;
            ofs << content;
        }
    }

    void populate_build_targets(BuildManager& manager) {
        std::vector<std::pair<std::string, std::string>> entries = {
            { "M.cppm", "M.o" },
            { "Impl.cpp", "Impl.o" },
            { "impl_part.cppm", "impl_part.o" },
            { "interface_part.cppm", "interface_part.o" },
            { "User.cpp", "User.o" }
        };

        for (const auto& [file, output] : entries) {
            std::vector<std::string> cflags = { "-std=c++20" };
            CompilationUnitType type = file.ends_with(".cppm")
                ? CompilationUnitType::Module
                : CompilationUnitType::Source;

            CompilationFlags compilation_flags;
            compilation_flags.cflags = cflags;
            manager.add_compilation_target(
                file,
                output,
                compilation_flags,
                type);
        }
    }
};

TEST_F(ModuleResolutionTest, CanResolveModules) {
    muuk::resolve_modules(manager, temp_dir.string());

    // Assert at least one logical name got resolved
    // TODO: Check specific logical names
    auto& targets = manager.get_compilation_targets();
    bool found_logical_name = false;
    for (const auto& t : targets) {
        if (!t.logical_name.empty()) {
            found_logical_name = true;
            muuk::logger::info("Resolved module: {}", t.logical_name);
        }
    }

    ASSERT_TRUE(found_logical_name) << "No logical module names were resolved.";
}
