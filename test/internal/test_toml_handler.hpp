#include <string>

#include <gtest/gtest.h>
#include <toml++/toml.h>

#include "muuk_toml_handler.hpp"

// Sample TOML data for testing
const std::string sample_toml = R"(
[package]
name = "muuk"
version = "1.0.0"

[[dependencies]]
name = "fmt"
version = "9.0.0"

[[dependencies]]
name = "spdlog"
version = "1.8.5"
)";

// Test fixture class
class MuukTomlHandlerTest : public ::testing::Test {
protected:
    MuukTomlHandler handler;

    void SetUp() override {
        // Parse sample TOML content before each test
        auto result = MuukTomlHandler::parse(sample_toml);
        if (result) {
            handler = result.value();
        }
        ASSERT_TRUE(result);
    }
};

TEST_F(MuukTomlHandlerTest, ParseTOMLContent) {
    ASSERT_TRUE(handler.has_section("package"));
    ASSERT_TRUE(handler.has_section("dependencies"));

    auto package = handler.get_section("package");
    ASSERT_TRUE(package.has_value());
    EXPECT_EQ(package->get().as_table()["name"].as_string()->get(), "muuk");
    EXPECT_EQ(package->get().as_table()["version"].as_string()->get(), "1.0.0");
}

TEST_F(MuukTomlHandlerTest, HasSection) {
    EXPECT_TRUE(handler.has_section("package"));
    EXPECT_TRUE(handler.has_section("dependencies"));
    EXPECT_FALSE(handler.has_section("nonexistent_section"));
}

TEST_F(MuukTomlHandlerTest, GetSection) {
    auto package = handler.get_section("package");
    ASSERT_TRUE(package.has_value());
    EXPECT_EQ(package->get().as_table()["name"].as_string()->get(), "muuk");

    auto dependencies = handler.get_section("dependencies");
    ASSERT_TRUE(dependencies.has_value());
}

TEST_F(MuukTomlHandlerTest, ModifySection) {
    auto package = handler.get_section("package");
    ASSERT_TRUE(package.has_value());

    package->get().as_table().insert("description", "A powerful build system");
    EXPECT_EQ(package->get().as_table()["description"].as_string()->get(), "A powerful build system");
}

TEST_F(MuukTomlHandlerTest, SerializeTOML) {
    std::string serialized = handler.serialize();

    // Ensure essential lines exist
    EXPECT_NE(serialized.find("[package]"), std::string::npos);
    EXPECT_NE(serialized.find("name = 'muuk'"), std::string::npos);
    EXPECT_NE(serialized.find("version = '1.0.0'"), std::string::npos);
    EXPECT_NE(serialized.find("[[dependencies]]"), std::string::npos);
}

TEST_F(MuukTomlHandlerTest, AddNewSection) {
    toml::table new_section;
    new_section.insert("key1", "value1");
    handler.set_section("new_section", new_section);

    EXPECT_TRUE(handler.has_section("new_section"));

    auto section = handler.get_section("new_section");
    ASSERT_TRUE(section.has_value());
    EXPECT_EQ(section->get().as_table()["key1"].as_string()->get(), "value1");
}

TEST_F(MuukTomlHandlerTest, AddNestedTable) {
    toml::table nested;
    nested.insert("key1", "val1");
    nested.insert("key2", 42);

    auto package = handler.get_section("package");
    ASSERT_TRUE(package.has_value());

    package->get().as_table().insert("nested_table", nested);

    std::string serialized = handler.serialize();
    EXPECT_NE(serialized.find("nested_table = { key1 = 'val1', key2 = 42 }"), std::string::npos);
}

TEST_F(MuukTomlHandlerTest, AddArrayOfTables) {
    std::vector<toml::table> new_deps;
    toml::table dep1;
    dep1.insert("name", "boost");
    dep1.insert("version", "1.75.0");
    new_deps.push_back(dep1);

    toml::table dep2;
    dep2.insert("name", "json");
    dep2.insert("version", "3.10.2");
    new_deps.push_back(dep2);

    handler.set_section("extra_dependencies", new_deps);

    EXPECT_TRUE(handler.has_section("extra_dependencies"));

    std::string serialized = handler.serialize();
    EXPECT_NE(serialized.find("[[extra_dependencies]]"), std::string::npos);
    EXPECT_NE(serialized.find("name = 'boost'"), std::string::npos);
    EXPECT_NE(serialized.find("version = '1.75.0'"), std::string::npos);
}

TEST_F(MuukTomlHandlerTest, SectionOrderPreserved) {
    std::string serialized = handler.serialize();
    size_t package_pos = serialized.find("[package]");
    size_t dependencies_pos = serialized.find("[[dependencies]]");

    ASSERT_NE(package_pos, std::string::npos);
    ASSERT_NE(dependencies_pos, std::string::npos);
    EXPECT_LT(package_pos, dependencies_pos); // Ensure package appears before dependencies
}
