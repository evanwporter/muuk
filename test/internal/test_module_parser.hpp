#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

#include "buildmanager.h"
#include "muuk.h"

namespace muuk::util::command_line {
    std::string execute_command_get_out(const std::string& cmd);
}

// === MOCK command execution to return the clang-scan-deps output ===

namespace {
    std::string mock_scan_deps_output;

    std::string muuk::util::command_line::execute_command_get_out(const std::string&) {
        return mock_scan_deps_output;
    }

    const std::string test_build_dir = "test_build_dir";

    void ensure_build_dir_exists() {
        std::filesystem::create_directory(test_build_dir);
    }

    // Helper for finding a CompilationTarget by output
    CompilationTarget* find_target(BuildManager& bm, const std::string& output) {
        return bm.find_compilation_target("output", output);
    }

    // Helper to assert a dependency exists
    void assert_depends_on(CompilationTarget* source, CompilationTarget* expected) {
        ASSERT_NE(source, nullptr);
        ASSERT_NE(expected, nullptr);
        auto& deps = source->dependencies;
        EXPECT_NE(std::find(deps.begin(), deps.end(), expected), deps.end())
            << "Expected dependency not found!";
    }
}

TEST(ModuleResolutionTest, ResolvesComplexModuleGraphCorrectly) {
    ensure_build_dir_exists();

    BuildManager build_manager;

    // === 1. Add all compilation targets ===
    build_manager.add_compilation_target("M.cppm", "M.o", {"-std=c++20"}, {});
    build_manager.add_compilation_target("Impl.cpp", "Impl.o", {"-std=c++20"}, {});
    build_manager.add_compilation_target("impl_part.cppm", "impl_part.o", {"-std=c++20"}, {});
    build_manager.add_compilation_target("interface_part.cppm", "interface_part.o", {"-std=c++20"}, {});
    build_manager.add_compilation_target("User.cpp", "User.o", {"-std=c++20"}, {});

    // === 2. Provide mock clang-scan-deps output ===
    mock_scan_deps_output = R"json(
    {
      "revision": 0,
      "version": 1,
      "rules": [
        {
          "primary-output": "Impl.o",
          "requires": [
            {
              "logical-name": "M",
              "source-path": "M.cppm"
            }
          ]
        },
        {
          "primary-output": "M.o",
          "provides": [
            {
              "is-interface": true,
              "logical-name": "M",
              "source-path": "M.cppm"
            }
          ],
          "requires": [
            {
              "logical-name": "M:interface_part",
              "source-path": "interface_part.cppm"
            },
            {
              "logical-name": "M:impl_part",
              "source-path": "impl_part.cppm"
            }
          ]
        },
        {
          "primary-output": "User.o",
          "requires": [
            {
              "logical-name": "M",
              "source-path": "M.cppm"
            },
            {
              "logical-name": "third_party_module"
            }
          ]
        },
        {
          "primary-output": "impl_part.o",
          "provides": [
            {
              "is-interface": false,
              "logical-name": "M:impl_part",
              "source-path": "impl_part.cppm"
            }
          ],
          "requires": [
            {
              "logical-name": "M:interface_part",
              "source-path": "interface_part.cppm"
            }
          ]
        },
        {
          "primary-output": "interface_part.o",
          "provides": [
            {
              "is-interface": true,
              "logical-name": "M:interface_part",
              "source-path": "interface_part.cppm"
            }
          ]
        }
      ]
    })json";

    // === 3. Run resolution ===
    muuk::resolve_modules(build_manager, test_build_dir);

    // === 4. Validate dependencies ===
    auto* user      = find_target(build_manager, "User.o");
    auto* impl      = find_target(build_manager, "Impl.o");
    auto* m         = find_target(build_manager, "M.o");
    auto* impl_part = find_target(build_manager, "impl_part.o");
    auto* iface     = find_target(build_manager, "interface_part.o");

    // user → M
    assert_depends_on(user, m);

    // impl → M
    assert_depends_on(impl, m);

    // M → impl_part, interface_part
    assert_depends_on(m, impl_part);
    assert_depends_on(m, iface);

    // impl_part → interface_part
    assert_depends_on(impl_part, iface);
}
