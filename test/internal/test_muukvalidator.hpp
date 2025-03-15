#include "../../include/muukvalidator.hpp"
#include "compiler.hpp"

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

    EXPECT_TRUE(muuk::validate_muuk_toml(valid_toml).has_value());
}

TEST(ValidateMuukTomlTest, MissingRequiredKey) {
    toml::table invalid_toml = toml::parse(R"([package]
        version = "1.0"
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, TypeMismatch) {
    toml::table invalid_toml = toml::parse(R"([package]
        version = 1.0
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidArrayType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        authors = ["Author1", 2]
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidNestedTable) {
    toml::table invalid_toml = CREATE_TOML(R"(
        [library]
        include = "include"
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidBooleanType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = true
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidDateType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidTimeType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 12:00:00
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidDateTimeType) {
    toml::table invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01T12:00:00Z
    )");

    auto result = muuk::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

class DependencyNameTest : public ::testing::Test { };

TEST_F(DependencyNameTest, ValidNames) {
    EXPECT_TRUE(muuk::is_valid_dependency_name("valid-name"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("package_name"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("pkg-name"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("gtkmm-4.0")); // Digits wrapping `.`
    EXPECT_TRUE(muuk::is_valid_dependency_name("ncurses++")); // Double `+` allowed
    EXPECT_TRUE(muuk::is_valid_dependency_name("pkg/name")); // Single `/` allowed
    EXPECT_TRUE(muuk::is_valid_dependency_name("libboost_1.76"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("test-1.2.3"));
}

TEST_F(DependencyNameTest, InvalidNames) {
    EXPECT_FALSE(muuk::is_valid_dependency_name("")); // Empty name
    EXPECT_FALSE(muuk::is_valid_dependency_name("-start")); // Starts with `-`
    EXPECT_FALSE(muuk::is_valid_dependency_name("_start")); // Starts with `_`
    EXPECT_FALSE(muuk::is_valid_dependency_name("/start")); // Starts with `/`
    EXPECT_FALSE(muuk::is_valid_dependency_name("+start")); // Starts with `+`
    EXPECT_FALSE(muuk::is_valid_dependency_name("end-")); // Ends with `-`
    EXPECT_FALSE(muuk::is_valid_dependency_name("end_")); // Ends with `_`
    EXPECT_FALSE(muuk::is_valid_dependency_name("end/")); // Ends with `/`
    EXPECT_FALSE(muuk::is_valid_dependency_name("end++-")); // Ends with `++-`
    EXPECT_FALSE(muuk::is_valid_dependency_name("a.b.c")); // `.` not wrapped by digits
    EXPECT_FALSE(muuk::is_valid_dependency_name("a..b")); // Consecutive `.`
    EXPECT_FALSE(muuk::is_valid_dependency_name("a--b")); // Consecutive `-`
    EXPECT_FALSE(muuk::is_valid_dependency_name("a__b")); // Consecutive `_`
    EXPECT_FALSE(muuk::is_valid_dependency_name("a//b")); // Consecutive `/`
}

TEST_F(DependencyNameTest, InvalidSpecialCharacters) {
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg@name")); // `@` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg#name")); // `#` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg$name")); // `$` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg%name")); // `%` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg^name")); // `^` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg&name")); // `&` is invalid
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg*name")); // `*` is invalid
}

TEST_F(DependencyNameTest, ValidEdgeCases) {
    EXPECT_TRUE(muuk::is_valid_dependency_name("a1")); // Smallest valid name
    EXPECT_TRUE(muuk::is_valid_dependency_name("a-b")); // Single `-`
    EXPECT_TRUE(muuk::is_valid_dependency_name("a_b")); // Single `_`
    EXPECT_TRUE(muuk::is_valid_dependency_name("a/b")); // Single `/`
    // EXPECT_TRUE(muuk::is_valid_dependency_name("a.1"));       // Digit-wrapped `.`
    EXPECT_TRUE(muuk::is_valid_dependency_name("1.2.3")); // Version style valid
    EXPECT_TRUE(muuk::is_valid_dependency_name("libc++")); // Exactly two `+`
    EXPECT_TRUE(muuk::is_valid_dependency_name("ncurses++")); // Consecutive `+`
}

TEST_F(DependencyNameTest, InvalidPlusSigns) {
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg+name")); // Only one `+`
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg+++name")); // More than two `+`
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg+a+b")); // Non-consecutive `+`
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg+1+")); // `+` not consecutive
}

TEST_F(DependencyNameTest, InvalidSlash) {
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg/name/test")); // More than one `/`
    EXPECT_FALSE(muuk::is_valid_dependency_name("/pkg/name")); // Starts with `/`
    EXPECT_FALSE(muuk::is_valid_dependency_name("pkg/name/")); // Ends with `/`
}

TEST_F(DependencyNameTest, ValidLongNames) {
    EXPECT_TRUE(muuk::is_valid_dependency_name("super-long-package-name-1.2.3"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("valid_name_with_under_scores"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("complex-pkg_1.0.0-alpha"));
}

TEST_F(DependencyNameTest, InvalidLongNames) {
    EXPECT_FALSE(muuk::is_valid_dependency_name("invalid__pkg_name")); // Double `_`
    EXPECT_FALSE(muuk::is_valid_dependency_name("invalid--pkg-name")); // Double `-`
    EXPECT_FALSE(muuk::is_valid_dependency_name("invalid..pkg.name")); // Double `.`
    EXPECT_FALSE(muuk::is_valid_dependency_name("invalid/name/with/multiple/slashes"));
}

TEST_F(DependencyNameTest, InvalidEmptyOrSpaces) {
    EXPECT_FALSE(muuk::is_valid_dependency_name(" ")); // Single space
    EXPECT_FALSE(muuk::is_valid_dependency_name(" a")); // Leading space
    EXPECT_FALSE(muuk::is_valid_dependency_name("a ")); // Trailing space
    EXPECT_FALSE(muuk::is_valid_dependency_name("a b")); // Space in the middle
    EXPECT_FALSE(muuk::is_valid_dependency_name("\tname")); // Tab in name
}

TEST_F(DependencyNameTest, ValidMixedCases) {
    EXPECT_TRUE(muuk::is_valid_dependency_name("test-lib_1.0.0"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("pkg/boost_1.82"));
    EXPECT_TRUE(muuk::is_valid_dependency_name("foo-bar_2.1.4"));
}

class ValidateFlagTest : public ::testing::Test { };

TEST_F(ValidateFlagTest, ValidMSVCFlags) {
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/O2").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/W3").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/EHsc").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/std:c++20").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/MD").has_value());
}

TEST_F(ValidateFlagTest, InvalidMSVCFlags) {
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "").has_value()); // Empty
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "-O2").has_value()); // Wrong prefix
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "O2").has_value()); // Missing `/`
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "/flag!").has_value()); // Invalid char
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "/Werror$").has_value()); // Invalid char
}

TEST_F(ValidateFlagTest, ValidGCCClangFlags) {
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-O2").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-Wall").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-Werror").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-std=c++17").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-DDEBUG").has_value());
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-march=native").has_value());
}

TEST_F(ValidateFlagTest, InvalidGCCClangFlags) {
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "").has_value()); // Empty
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "/O2").has_value()); // Wrong prefix
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "O2").has_value()); // Missing `-`
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "-flag!").has_value()); // Invalid char
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "-Wno$errors").has_value()); // Invalid char
}

TEST_F(ValidateFlagTest, EdgeCases) {
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::MSVC, "/D").has_value()); // Smallest valid flag (MSVC)
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-D").has_value()); // Smallest valid flag (GCC)
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-fno-exceptions").has_value()); // Long flag
    EXPECT_TRUE(muuk::validate_flag(muuk::Compiler::GCC, "-march=x86-64").has_value()); // `-` inside flag
}

TEST_F(ValidateFlagTest, OnlyPrefixInvalid) {
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::MSVC, "-").has_value()); // Only `/`
    EXPECT_FALSE(muuk::validate_flag(muuk::Compiler::GCC, "/").has_value()); // Only `-`
}