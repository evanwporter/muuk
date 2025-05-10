#pragma once
#ifndef TEST_BUILD_MANAGER_HPP
#define TEST_BUILD_MANAGER_HPP
#include <algorithm>

#include <gtest/gtest.h>

#include "../../include/compiler.hpp"
#include "build/manager.hpp"
#include "build/targets.hpp"

using namespace muuk;
using namespace muuk::build;

class BuildManagerTest : public ::testing::Test {
protected:
    BuildManager build_manager;
};

// Test that BuildManager initializes correctly
TEST_F(BuildManagerTest, Initialization) {
    EXPECT_TRUE(build_manager.get_compilation_targets().empty());
    EXPECT_TRUE(build_manager.get_archive_targets().empty());
    EXPECT_TRUE(build_manager.get_link_targets().empty());
}

// Test adding a compilation target
TEST_F(BuildManagerTest, AddCompilationTarget) {
    CompilationFlags compilation_flags;
    compilation_flags.cflags = { "-O2" };
    compilation_flags.iflags = { "-Iinclude" };

    build_manager.add_compilation_target("source.cpp", "source.o", compilation_flags);

    auto& compilation_targets = build_manager.get_compilation_targets();
    ASSERT_EQ(compilation_targets.size(), 1);
    EXPECT_EQ(compilation_targets[0].inputs[0], "source.cpp");
    EXPECT_EQ(compilation_targets[0].output, "source.o");
    EXPECT_EQ(compilation_targets[0].flags, std::vector<std::string>({ "-O2", "-Iinclude" }));
}

// TODO: Raise Err for double adding?
// Test adding a duplicate compilation target (should not add it twice)
TEST_F(BuildManagerTest, AddDuplicateCompilationTarget) {
    CompilationFlags compilation_flags1, compilation_flags2;
    compilation_flags1.cflags = { "-O2" };
    compilation_flags1.iflags = { "-Iinclude" };

    compilation_flags1.cflags = { "-O3" };
    compilation_flags1.iflags = { "-Ilib" };

    build_manager.add_compilation_target("source.cpp", "source.o", compilation_flags1);
    build_manager.add_compilation_target("source.cpp", "source.o", compilation_flags2);

    auto& compilation_targets = build_manager.get_compilation_targets();
    EXPECT_EQ(compilation_targets.size(), 1); // Should only contain one entry
}

// Test adding an archive target
TEST_F(BuildManagerTest, AddArchiveTarget) {
    build_manager.add_archive_target("libmylib.a", { "source.o", "utils.o" }, { "rcs" });

    auto archive_targets = build_manager.get_archive_targets();
    ASSERT_EQ(archive_targets.size(), 1);
    EXPECT_EQ(archive_targets[0].output, "libmylib.a");
    EXPECT_EQ(archive_targets[0].inputs, std::vector<std::string>({ "source.o", "utils.o" }));
    EXPECT_EQ(archive_targets[0].flags, std::vector<std::string>({ "rcs" }));
}

// Test adding a duplicate archive target (should not add it twice)
TEST_F(BuildManagerTest, AddDuplicateArchiveTarget) {
    build_manager.add_archive_target("libmylib.a", { "source.o" }, { "rcs" });
    build_manager.add_archive_target("libmylib.a", { "utils.o" }, { "rcs" });

    auto archive_targets = build_manager.get_archive_targets();
    EXPECT_EQ(archive_targets.size(), 1); // Should only contain one entry
}

// Test adding a link target
TEST_F(BuildManagerTest, AddLinkTarget) {
    build_manager.add_link_target("myprogram", { "source.o", "utils.o" }, { "libmylib.a" }, { "-Llib" }, BuildLinkType::EXECUTABLE);

    auto link_targets = build_manager.get_link_targets();
    ASSERT_EQ(link_targets.size(), 1);
    EXPECT_EQ(link_targets[0].output, "myprogram");
    EXPECT_EQ(link_targets[0].inputs, std::vector<std::string>({ "source.o", "utils.o", "libmylib.a" }));
    EXPECT_EQ(link_targets[0].flags, std::vector<std::string>({ "-Llib" }));
}

// Test adding compilation target with empty file paths
TEST_F(BuildManagerTest, AddEmptyCompilationTarget) {
    build_manager.add_compilation_target("", "", {}, {});
    EXPECT_TRUE(build_manager.get_compilation_targets().empty()); // Should not add an empty entry
}

// Test adding archive target with empty file paths
TEST_F(BuildManagerTest, AddEmptyArchiveTarget) {
    build_manager.add_archive_target("", {}, {});
    EXPECT_TRUE(build_manager.get_archive_targets().empty()); // Should not add an empty entry
}

// Test adding link target with empty file paths
TEST_F(BuildManagerTest, AddEmptyLinkTarget) {
    build_manager.add_link_target("", {}, {}, {}, BuildLinkType::EXECUTABLE);
    EXPECT_TRUE(build_manager.get_link_targets().empty()); // Should not add an empty entry
}

// Test circular dependency where an object depends on itself
TEST_F(BuildManagerTest, CircularDependency) {
    build_manager.add_compilation_target("source.cpp", "source.o", {}, {});
    build_manager.add_link_target("source.o", { "source.o" }, {}, {}, BuildLinkType::EXECUTABLE);

    auto link_targets = build_manager.get_link_targets();
    ASSERT_EQ(link_targets.size(), 1);

    // Check if output is listed as an input
    EXPECT_NE(std::find(link_targets[0].inputs.begin(), link_targets[0].inputs.end(), "source.o"), link_targets[0].inputs.end());
}

// Test providing incorrect object files in an archive
TEST_F(BuildManagerTest, ArchiveTargetWithInvalidFiles) {
    build_manager.add_archive_target("libwrong.a", { "source.cpp" }, { "rcs" });

    auto archive_targets = build_manager.get_archive_targets();
    ASSERT_EQ(archive_targets.size(), 1);

    // Ensure that the input is not an object file
    EXPECT_EQ(archive_targets[0].inputs[0], "source.cpp");
}

// Test adding two compilation targets with the same object file but different sources
TEST_F(BuildManagerTest, ConflictingObjectFiles) {
    CompilationFlags compilation_flags;
    compilation_flags.cflags = { "-O2" };
    compilation_flags.iflags = { "-Iinclude" };

    build_manager.add_compilation_target("source1.cpp", "shared.o", compilation_flags);
    build_manager.add_compilation_target("source2.cpp", "shared.o", compilation_flags);

    auto& compilation_targets = build_manager.get_compilation_targets();

    // Only one should exist because they produce the same .o file
    EXPECT_EQ(compilation_targets.size(), 1);
    EXPECT_EQ(compilation_targets[0].inputs[0], "source1.cpp");
}

// Test adding an archive target with duplicate object files
TEST_F(BuildManagerTest, DuplicateObjectFilesInArchive) {
    build_manager.add_archive_target("libdup.a", { "shared.o", "shared.o" }, { "rcs" });

    auto archive_targets = build_manager.get_archive_targets();
    ASSERT_EQ(archive_targets.size(), 1);

    // Ensure only one instance of the duplicate object remains
    std::vector<std::string> unique_objects(archive_targets[0].inputs.begin(), archive_targets[0].inputs.end());
    std::sort(unique_objects.begin(), unique_objects.end());
    unique_objects.erase(std::unique(unique_objects.begin(), unique_objects.end()), unique_objects.end());

    EXPECT_EQ(unique_objects.size(), 1);
}

// Test adding an executable that links against itself
TEST_F(BuildManagerTest, ExecutableLinksToItself) {
    build_manager.add_link_target("self_exec", { "self_exec" }, {}, {}, BuildLinkType::EXECUTABLE);

    auto link_targets = build_manager.get_link_targets();
    ASSERT_EQ(link_targets.size(), 1);

    // Ensure executable is incorrectly listed as an input
    EXPECT_NE(std::find(link_targets[0].inputs.begin(), link_targets[0].inputs.end(), "self_exec"), link_targets[0].inputs.end());
}

#endif // TEST_BUILD_MANAGER_HPP
