#include <gtest/gtest.h>
#include <iostream>
#include "../../include/muuker.hpp"
#include "../../include/logger.h"
#include "../../include/util.h"
#include "../../include/muukfiler.h"

class MuukTests : public ::testing::Test {
protected:
    // SetUpTestSuite runs once before any test in the suite is executed.
    static void SetUpTestSuite() {
        // Logger::initialize(); // Initialize the logger once for the entire test suite
    }

    void SetUp() override {
        // Any per-test setup can go here
    }
};

// Test Case 1: Configuration Retrieval
TEST(MuukTests, GetConfig) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    const auto& config = muukFiler.get_config();
    EXPECT_TRUE(config.contains("package"));
}

// Test Case 2: Has Section
TEST(MuukTests, HasSection) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    EXPECT_TRUE(muukFiler.has_section("package"));
    EXPECT_TRUE(muukFiler.has_section("library.test"));

    // if (muukFiler.has_section("package")) {
    //     const auto& package_section = muukFiler.get_section("package").as_table();
    //     if (package_section && package_section->contains("name")) {
    //         std::string package_name = package_section->get("name")->value<std::string>().value_or("");
    //         std::string library_section = "library." + package_name;
    //         EXPECT_TRUE(muukFiler.has_section(library_section));
    //     }
    // }
}

// Test Case 3: Get Section
TEST(MuukTests, GetSection) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    auto section = muukFiler.get_section("package");
    EXPECT_FALSE(section.empty());
}

// Test Case 4: Update Section
TEST(MuukTests, UpdateSection) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    toml::table new_data{
        {"new_script", "echo New Script"}
    };
    muukFiler.modify_section("scripts", new_data);

    auto updated_section = muukFiler.get_section("scripts");
    EXPECT_TRUE(updated_section.contains("new_script"));
}

// Test Case 5: Run Script Successfully
TEST(MuukTests, RunScript) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    // muuk::logger::info("Modding config");
    std::cout << "Modding config" << std::endl;

    muukFiler.modify_section("scripts", toml::table{
        {"greet", "echo Hello, Test!"}
        });

    std::cout << "Logging output" << std::endl;

    std::string output_file = "test_output.txt";

    std::remove(output_file.c_str());

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

// Test Case 6: Clean Operation with Matching Files
TEST(MuukTests, CleanOperation) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    muukFiler.modify_section("clean", toml::table{
        {"patterns", toml::array{"*.tmp"}}
        });

    std::ofstream("test.tmp").close(); // Create a dummy file

    muuk.clean();

    EXPECT_FALSE(std::filesystem::exists("test.tmp"));
    // EXPECT_FALSE(std::filesystem::exists("test.log"));
}

// Test Case 7: Ensure Configuration Updates Persist
TEST(MuukTests, UpdateConfigPersists) {
    MuukFiler muukFiler;
    Muuker muuk(muukFiler);

    toml::table new_data{
        {"build_command", "g++ -o output main.cpp"}
    };
    muukFiler.modify_section("build", new_data);

    auto updated_section = muukFiler.get_section("build");
    EXPECT_TRUE(updated_section.contains("build_command"));
    EXPECT_EQ(updated_section["build_command"].value<std::string>(), "g++ -o output main.cpp");
}