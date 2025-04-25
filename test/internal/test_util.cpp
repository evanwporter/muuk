#include "../../include/util.h"
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class UtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_directory = "test_dir";
        if (fs::exists(test_directory)) {
            fs::remove_all(test_directory);
        }
    }

    void TearDown() override {
        if (fs::exists(test_directory)) {
            fs::remove_all(test_directory);
        }
    }

    std::string test_directory;
};

// **Test Case: Ensure Directory Exists**
TEST_F(UtilTest, EnsureDirectoryExists_CreatesDirectory) {
    std::string test_dir = test_directory + "/new_dir";
    ASSERT_FALSE(fs::exists(test_dir)) << "Test directory should not exist before calling file_system::ensure_directory_exists";
    util::file_system::ensure_directory_exists(test_dir, false);
    EXPECT_TRUE(fs::exists(test_dir)) << "Test directory should be created by file_system::ensure_directory_exists";
    fs::remove_all(test_dir);
}

// **Test Case: Ensure .gitignore Creation**
TEST_F(UtilTest, EnsureDirectoryExists_WithGitignore) {
    std::string gitignore_file = test_directory + "/.gitignore";

    util::file_system::ensure_directory_exists(test_directory, true);

    EXPECT_TRUE(fs::exists(test_directory));
    EXPECT_TRUE(fs::exists(gitignore_file));

    // Verify .gitignore content
    std::ifstream file(gitignore_file);
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, "*");
}

// **Test Case: Path Exists**
TEST_F(UtilTest, PathExists) {
    fs::create_directories(test_directory);
    EXPECT_TRUE(util::file_system::path_exists(test_directory));
    EXPECT_FALSE(util::file_system::path_exists("non_existent_path"));
}

// **Test Case: Execute System Command**
TEST(UtilCommandTest, ExecuteCommand) {
    int result = util::command_line::execute_command("echo Hello");
    EXPECT_EQ(result, 0); // System commands return 0 on success
}

// ** Test Case: Execute Non-Existent Command **
TEST(UtilCommandTest, ExecuteInvalidCommand) {
    int result = util::command_line::execute_command("invalid_command_that_does_not_exist");
    EXPECT_NE(result, 0); // Expect a failure result
}

// ** Test Case: Ensure .gitignore Exists With Edge Cases **
TEST_F(UtilTest, EnsureGitignoreCreationEdgeCases) {
    std::string path = test_directory + "/subfolder";

    util::file_system::ensure_directory_exists(path, true);
    std::string gitignore_path = path + "/.gitignore";

    EXPECT_TRUE(fs::exists(gitignore_path));

    std::ifstream file(gitignore_path);
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, "*");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
