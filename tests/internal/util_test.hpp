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
    int result = util::execute_command("echo Hello");
    EXPECT_EQ(result, 0);  // System commands return 0 on success
}

// **Test Case: UTF-8 Validation**
TEST(UtilStringTest, IsValidUTF8) {
    EXPECT_TRUE(util::is_valid_utf8("Hello World"));
    EXPECT_FALSE(util::is_valid_utf8(std::string("\xFF\xFF\xFF")));  // Invalid UTF-8
}

// **Test Case: Convert to UTF-8**
TEST(UtilStringTest, ConvertToUTF8) {
    std::wstring wide_str = L"Hello, 世界";
    std::string utf8_str = util::to_utf8(wide_str);
    EXPECT_FALSE(utf8_str.empty());
}

// **Mocked Tests for HTTP & Zip Extraction** (Avoid real file downloads or extractions)

// // **Mock File Download**
// class MockFileDownloader {
// public:
//     MOCK_METHOD(void, download_file, (const std::string&, const std::string&), ());
// };

// TEST(UtilDownloadTest, DownloadFile) {
//     // Mock file download
//     MockFileDownloader mockDownloader;
//     EXPECT_CALL(mockDownloader, download_file(_, _)).Times(1);

//     try {
//         util::download_file("https://example.com/file.zip", "test.zip");
//     }
//     catch (...) {
//         // Ignore actual download failures
//     }
// }

// // **Mock GitHub JSON Fetch**
// class MockJsonFetcher {
// public:
//     MOCK_METHOD(nlohmann::json, fetch_json, (const std::string&), ());
// };

// TEST(UtilNetworkTest, FetchJson) {
//     MockJsonFetcher mockFetcher;
//     nlohmann::json fake_response = { {"tag_name", "v1.2.3"} };

//     EXPECT_CALL(mockFetcher, fetch_json(_))
//         .WillOnce(Return(fake_response));

//     try {
//         nlohmann::json result = util::fetch_json("https://api.github.com/repos/user/repo/releases/latest");
//         EXPECT_EQ(result["tag_name"], "v1.2.3");
//     }
//     catch (...) {
//         // Ignore actual HTTP failures
//     }
// }

// // **Mock ZIP Extraction**
// class MockZipExtractor {
// public:
//     MOCK_METHOD(void, extract_zip, (const std::string&, const std::string&), ());
// };

// TEST(UtilZipTest, ExtractZip) {
//     MockZipExtractor mockExtractor;
//     EXPECT_CALL(mockExtractor, extract_zip(_, _)).Times(1);

//     try {
//         util::extract_zip("test.zip", "output_folder");
//     }
//     catch (...) {
//         // Ignore extraction failures
//     }
// }

// // **Main Function for Google Test**
// int main(int argc, char** argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
