#include <gtest/gtest.h>
#include "mockmuukfiler.cpp"
#include <iostream>
#include "../../include/muuk.h"
#include "../../include/logger.h"
#include "../../include/util.h"

class MuukTests : public ::testing::Test {
protected:
    // SetUpTestSuite runs once before any test in the suite is executed.
    static void SetUpTestSuite() {
        Logger::initialize(); // Initialize the logger once for the entire test suite
    }

    void SetUp() override {
        // Any per-test setup can go here
    }
};

// ** Test Case 1: Configuration Retrieval **
TEST(MuukTests, GetConfig) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);

    const auto& config = mockFiler.get_config();
    EXPECT_TRUE(config.contains("scripts"));
    EXPECT_TRUE(config.contains("clean"));
}

// ** Test Case 2: Has Section **
TEST(MuukTests, HasSection) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);

    EXPECT_TRUE(mockFiler.has_section("scripts"));
    EXPECT_FALSE(mockFiler.has_section("nonexistent"));
}

// ** Test Case 3: Get Section **
TEST(MuukTests, GetSection) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);

    auto section = mockFiler.get_section("scripts");
    EXPECT_FALSE(section.empty());
}

// ** Test Case 4: Update Section **
TEST(MuukTests, UpdateSection) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);

    toml::table new_data{
        {"new_script", "echo New Script"}
    };
    mockFiler.update_section("scripts", new_data);

    auto updated_section = mockFiler.get_section("scripts");
    EXPECT_TRUE(updated_section.contains("new_script"));
}

// **Test Case 5: Run Script Successfully * *
TEST(MuukTests, RunScript) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);
    auto logger_ = Logger::get_logger("muuk_logger");

    // logger_->info("Modding config");
    std::cout << "Modding config" << std::endl;

    // Ensure mock configuration contains the script
    mockFiler.set_config(toml::table{
        {"scripts", toml::table{
            {"greet", "echo Hello, Test!"}// > test_output.txt"}
        }}
        });


    std::cout << "Logging output" << std::endl;

    std::string output_file = "test_output.txt";

    // Ensure file does not exist before execution
    std::remove(output_file.c_str());

    // Execute script
    muuk.run_script("greet", { ">", output_file });
    util::execute_command("echo hello world");

    // Ensure script ran before attempting to read
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Read the output file
    std::ifstream result(output_file);
    ASSERT_TRUE(result.good()) << "Output file does not exist!";

    std::string output;
    std::getline(result, output);
    result.close();

    // Validate script output
    EXPECT_TRUE(output.find("Hello, Test!") != std::string::npos);
}


// // ** Test Case 6: Run Nonexistent Script **
// TEST(MuukTests, RunNonExistentScript) {
//     MockMuukFiler mockFiler;
//     Muuk muuk(mockFiler);

//     // Customize config for this test
//     mockFiler.set_config(toml::table{
//         {"scripts", toml::table{}}  // No scripts defined
//         });

//     testing::internal::CaptureStdout();
//     muuk.run_script("invalid_script", {});
//     std::string output = testing::internal::GetCapturedStdout();
//     EXPECT_TRUE(output.find("Script 'invalid_script' not found") != std::string::npos);
// }

// ** Test Case 7: Clean Operation with Matching Files **
TEST(MuukTests, CleanOperation) {
    MockMuukFiler mockFiler;
    Muuk muuk(mockFiler);

    // Customize config for this test
    mockFiler.set_config(toml::table{
        {"clean", toml::table{
            {"patterns", toml::array{"*.tmp", "*.log"}}
        }}
        });


    std::ofstream("test.tmp").close(); // Create a dummy file
    std::ofstream("test.log").close();

    muuk.clean();

    EXPECT_FALSE(std::filesystem::exists("test.tmp"));
    EXPECT_FALSE(std::filesystem::exists("test.log"));
}
