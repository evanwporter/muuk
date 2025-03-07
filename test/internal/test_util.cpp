#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../include/util.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;

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
    ASSERT_FALSE(fs::exists(test_dir)) << "Test directory should not exist before calling ensure_directory_exists";
    util::ensure_directory_exists(test_dir, false);
    EXPECT_TRUE(fs::exists(test_dir)) << "Test directory should be created by ensure_directory_exists";
    fs::remove_all(test_dir);
}

// **Test Case: Ensure .gitignore Creation**
TEST_F(UtilTest, EnsureDirectoryExists_WithGitignore) {
    std::string gitignore_file = test_directory + "/.gitignore";

    util::ensure_directory_exists(test_directory, true);

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
    EXPECT_TRUE(util::path_exists(test_directory));
    EXPECT_FALSE(util::path_exists("non_existent_path"));
}

// **Test Case: Remove Path**
TEST_F(UtilTest, RemovePath) {
    fs::create_directories(test_directory);
    EXPECT_TRUE(fs::exists(test_directory));

    util::remove_path(test_directory);

    EXPECT_FALSE(fs::exists(test_directory));
}

// **Test Case: Pattern Matching**
TEST(UtilPatternTest, MatchPattern) {
    EXPECT_TRUE(util::match_pattern("build/main.o", "build/*"));
    EXPECT_TRUE(util::match_pattern("src/file.cpp", "src/*.cpp"));
    EXPECT_FALSE(util::match_pattern("docs/readme.md", "src/*.cpp"));
    EXPECT_FALSE(util::match_pattern("src/main.cpp", "!src/*.cpp"));
}

// **Test Case: Execute System Command**
TEST(UtilCommandTest, ExecuteCommand) {
    int result = util::command_line::execute_command("echo Hello");
    EXPECT_EQ(result, 0);  // System commands return 0 on success
}

// **Test Case: UTF-8 Validation**
// TEST(UtilStringTest, IsValidUTF8) {
//     EXPECT_TRUE(util::is_valid_utf8("Hello World"));
//     EXPECT_FALSE(util::is_valid_utf8(std::string("\xFF\xFF\xFF")));  // Invalid UTF-8
// }

// **Test Case: Convert to UTF-8**
TEST(UtilStringTest, ConvertToUTF8) {
    std::wstring wide_str = L"Hello, 世界";
    std::string utf8_str = util::to_utf8(wide_str);
    EXPECT_FALSE(utf8_str.empty());
}

// ** Test Case: Normalize Paths **
TEST(UtilPathTest, NormalizePath) {
    std::string path = "C:\\Users\\Test\\..\\Project\\file.txt";
    std::string normalized = util::normalize_path(path);

#ifdef _WIN32
    EXPECT_EQ(normalized, "C:/Users/Project/file.txt");
#else
    EXPECT_EQ(normalized, "/Users/Project/file.txt");
#endif
}

// ** Test Case: Execute Non-Existent Command **
TEST(UtilCommandTest, ExecuteInvalidCommand) {
    int result = util::command_line::execute_command("invalid_command_that_does_not_exist");
    EXPECT_NE(result, 0);  // Expect a failure result
}

// ** Test Case: Remove Non-Existent Path **
TEST(UtilPathTest, RemoveNonExistentPath) {
    std::string path = "non_existent_folder";
    EXPECT_NO_THROW(util::remove_path(path));
}

// ** Test Case: Ensure UTF-8 Validity Check Handles Edge Cases **
// TEST(UtilStringTest, UTF8EdgeCases) {
//     std::string valid_utf8 = "Hello, 世界";
//     std::string invalid_utf8 = std::string("\xFF\xFF\xFF");

//     EXPECT_TRUE(util::is_valid_utf8(valid_utf8));
//     EXPECT_FALSE(util::is_valid_utf8(invalid_utf8));
// }

// ** Test Case: Match Wildcards with Edge Cases **
TEST(UtilPatternTest, WildcardMatchingEdgeCases) {
    EXPECT_TRUE(util::match_pattern("src/main.cpp", "src/*.cpp"));
    EXPECT_FALSE(util::match_pattern("src/main.cpp", "build/*.cpp"));
    EXPECT_TRUE(util::match_pattern("build/debug/file.log", "build/**/*.log"));
}

// ** Test Case: Ensure .gitignore Exists With Edge Cases **
TEST_F(UtilTest, EnsureGitignoreCreationEdgeCases) {
    std::string path = test_directory + "/subfolder";

    util::ensure_directory_exists(path, true);
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
