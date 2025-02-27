#pragma once

#include <gtest/gtest.h>
#include "muukfiler.h"
#include "logger.h"
#include "muukvalidator.hpp"
#include <tl/expected.hpp>
#include <sstream>
#include <memory>

#include "./mocks/mockfileop.hpp"

// Helper function to create a default TOML content for testing
std::string get_sample_toml() {
    return R"(
        [package]
        name = "test_package"

        [dependencies]
        libA = { version = "1.2.3" }
        libB = { version = "4.5.6" }
    )";
}

// Test fixture class for MuukFiler
class MuukFilerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockFileOperations> mock_file_ops;
    std::unique_ptr<MuukFiler> muuk_filer;

    void SetUp() override {
        mock_file_ops = std::make_shared<MockFileOperations>();
        mock_file_ops->write_file(get_sample_toml()); // Set mock file content
        muuk_filer = std::make_unique<MuukFiler>(mock_file_ops);
    }
};

TEST_F(MuukFilerTest, ParseConfigFile) {
    std::string mock_config = R"(
        [section1]
        key1 = "value1"
        key2 = 42

        [section2]
        keyA = "valueA"
    )";

    mock_file_ops->write_file(mock_config);
    MuukFiler filer(mock_file_ops);

    EXPECT_TRUE(filer.has_section("section1"));
    EXPECT_TRUE(filer.has_section("section2"));
    EXPECT_FALSE(filer.has_section("non_existent"));

    toml::table section1 = filer.get_section("section1");
    EXPECT_EQ(section1["key1"].as_string()->get(), "value1");
    EXPECT_EQ(section1["key2"].as_integer()->get(), 42);
}

TEST_F(MuukFilerTest, ModifySection) {
    toml::table new_section;
    new_section.insert_or_assign("new_key", "new_value");

    muuk_filer->modify_section("new_section", new_section);

    toml::table retrieved_section = muuk_filer->get_section("new_section");
    EXPECT_EQ(retrieved_section["new_key"].as_string()->get(), "new_value");
}

TEST_F(MuukFilerTest, GetSectionOrder) {
    std::vector<std::string> order = muuk_filer->get_section_order();
    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], "package");
    EXPECT_EQ(order[1], "dependencies");
}

TEST_F(MuukFilerTest, WriteToFileAndVerifyContent) {
    std::string mock_config = R"(
[section2]
key1 = "value1"
key2 = 42

[section1]
keyA = "valueA"
key0 = "value0"
    )";

    mock_file_ops->write_file(mock_config);
    MuukFiler filer(mock_file_ops);

    EXPECT_TRUE(filer.has_section("section1"));
    EXPECT_TRUE(filer.has_section("section2"));

    muuk_filer->write_to_file();
    std::string written_content = mock_file_ops->read_file();

    EXPECT_EQ(written_content, mock_config);
}
