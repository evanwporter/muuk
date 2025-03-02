#include "../../include/muukvalidator.hpp"

#include <gtest/gtest.h>
#include <toml++/toml.h>

#define CREATE_TOML(str) toml::parse("[package]\nname = \"example\"\n" + std::string(str))

TEST(ValidateMuukTomlTest, ValidToml) {
    toml::table valid_toml = CREATE_TOML(R"(
        version = "1.0"
        description = "An example package"
        license = "MIT"
        authors = ["Author1", "Author2"]
        repository = "https://example.com/repo"
        documentation = "https://example.com/docs"
        homepage = "https://example.com"
        readme = "README.md"
        keywords = ["example", "test"]

        [library]
        include = ["include"]
        cflags = ["-O2"]
        system_include = ["system_include"]
        dependencies = {}
        [library.compiler]
        cflags = ["-Wall"]
        [library.platform]
        cflags = ["-DPLATFORM"]
    )");

    EXPECT_EQ(muuk::validate_muuk_toml(valid_toml), true);
}

TEST(ValidateMuukTomlTest, MissingRequiredKey) {
    toml::table invalid_toml = toml::parse(R"([package]
        version = "1.0"
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, TypeMismatch) {
    toml::table invalid_toml = toml::parse(R"([package]
        version = 1.0
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidArrayType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        authors = ["Author1", 2]
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidNestedTable) {
    toml::table invalid_toml = CREATE_TOML(R"(
        [library]
        include = "include"
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidBooleanType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = true
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidDateType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidTimeType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 12:00:00
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}

TEST(ValidateMuukTomlTest, InvalidDateTimeType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01T12:00:00Z
    )");

    EXPECT_THROW(muuk::validate_muuk_toml(invalid_toml), muuk::invalid_toml);
}
