#include "../../include/muukvalidator.hpp"
#include "compiler.hpp"

#include <gtest/gtest.h>
#include <toml.hpp>

#define CREATE_TOML(str) toml::parse_str("[package]\nname = \"example\"\n" + std::string(str))

TEST(ValidateMuukTomlTest, ValidToml) {
    toml::value valid_toml = CREATE_TOML(R"(
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

    EXPECT_TRUE(muuk::validation::validate_muuk_toml(valid_toml).has_value());
}

TEST(ValidateMuukTomlTest, MissingRequiredKey) {
    toml::value invalid_toml = toml::parse_str(R"([package]
        version = "1.0"
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, TypeMismatch) {
    toml::value invalid_toml = toml::parse_str(R"([package]
        version = 1.0
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidArrayType) {
    toml::value invalid_toml = CREATE_TOML(R"(
        authors = ["Author1", 2]
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidNestedTable) {
    toml::value invalid_toml = CREATE_TOML(R"(
        [library]
        include = "include"
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidBooleanType) {
    toml::value invalid_toml = CREATE_TOML(R"(
        version = true
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidDateType) {
    toml::value invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidTimeType) {
    toml::value invalid_toml = CREATE_TOML(R"(
        version = 12:00:00
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, InvalidDateTimeType) {
    toml::value invalid_toml = CREATE_TOML(R"(
        version = 2021-09-01T12:00:00Z
    )");

    auto result = muuk::validation::validate_muuk_toml(invalid_toml);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateMuukTomlTest, MixedTypeLibsArray) {
    toml::value valid_toml = toml::parse_str(R"(
        [package]
        name = "test"
        version = "1.0"

        [library]
        libs = [
            "foo.lib",
            { path = "bar.lib", platform = "windows" },
            { path = "baz.lib", compiler = "clang" }
        ]
    )");

    EXPECT_TRUE(muuk::validation::validate_muuk_toml(valid_toml).has_value());
}

TEST(ValidateMuukTomlTest, InvalidKeyMixedTypeLibsArray) {
    toml::value valid_toml = toml::parse_str(R"(
        [package]
        name = "test"
        version = "1.0"

        [library]
        libs = [
            "foo.lib",
            { name = "bar.lib", platform = "windows" },
            { path = "baz.lib", compiler = "clang" }
        ]
    )");

    EXPECT_FALSE(muuk::validation::validate_muuk_toml(valid_toml).has_value());
}

TEST(ValidateMuukTomlTest, MixedTypeSourcesArray) {
    toml::value valid_toml = toml::parse_str(R"(
        [package]
        name = "test"
        version = "1.0"

        [library]
        sources = [
            "alice.cpp",
            { path = "bar.cpp", cflags = ["-DDO_THIS"] },
            { path = "foo.cpp", cflags = ["-DNOT_THIS"] },
        ]
    )");

    EXPECT_TRUE(muuk::validation::validate_muuk_toml(valid_toml).has_value());
}

TEST(ValidateMuukTomlTest, InvalidLibsTableEntry) {
    toml::value invalid_toml = toml::parse_str(R"(
        [package]
        name = "test"
        version = "1.0"

        [library]
        libs = [
            { wrong_field = "x.lib" }
        ]
    )");

    EXPECT_FALSE(muuk::validation::validate_muuk_toml(invalid_toml).has_value());
}

// class ValidateMuukLockTomlTest : public ::testing::Test { };

// // TEST_F(ValidateMuukLockTomlTest, ValidMuukLockToml) {
// //     toml::table valid_lock_toml = toml::parse(R"(
// //         [[library]]
// //         name = "an_example"
// //         version = "1.2.3"
// //         source = "https://example.com/source"
// //         include = ["include"]
// //         cflags = ["-O2"]
// //         system_include = ["system/include"]

// //         [[library]]
// //         name = "another_example"
// //         version = "3.2.1"
// //         source = "https://example2.com/source"
// //         include = ["include2"]
// //         cflags = ["-O6"]
// //         system_include = ["system/include"]
// //     )");

// //     auto result = muuk::validate_muuk_lock_toml(valid_lock_toml);
// //     EXPECT_TRUE(result.has_value());
// // }

// TEST_F(ValidateMuukLockTomlTest, MissingRequiredField) {
//     toml::value invalid_lock_toml = toml::parse(R"(
//         [[library]]
//         version = "1.2.3"
//     )");

//     auto result = muuk::validate_muuk_lock_toml(invalid_lock_toml);
//     EXPECT_FALSE(result.has_value());
// }

// TEST_F(ValidateMuukLockTomlTest, TypeMismatch) {
//     toml::value invalid_lock_toml = toml::parse(R"(
//         [[library]]
//         name = "example_lib"
//         version = 1.2
//     )");

//     auto result = muuk::validate_muuk_lock_toml(invalid_lock_toml);
//     EXPECT_FALSE(result.has_value());
// }

// TEST_F(ValidateMuukLockTomlTest, InvalidArrayType) {
//     toml::value invalid_lock_toml = toml::parse(R"(
//         [[library]]
//         name = "example_lib"
//         version = "1.2.3"
//         dependencies = "invalid_string"
//     )");

//     auto result = muuk::validate_muuk_lock_toml(invalid_lock_toml);
//     EXPECT_FALSE(result.has_value());
// }

// TEST_F(ValidateMuukLockTomlTest, ValidBuildSection) {
//     toml::value valid_lock_toml = toml::parse(R"(
//         [[build]]
//         name = "example_build"
//         version = "1.0.0"
//         include = ["include"]
//         cflags = ["-Wall"]
//         system_include = ["system/include"]
//     )");

//     EXPECT_TRUE(muuk::validate_muuk_lock_toml(valid_lock_toml).has_value());
// }

// TEST_F(ValidateMuukLockTomlTest, InvalidBuildDependenciesType) {
//     toml::value invalid_lock_toml = toml::parse(R"(
//         [[build]]
//         name = "example_build"
//         version = "1.0.0"
//         dependencies = "not_an_array"
//     )");

//     auto result = muuk::validate_muuk_lock_toml(invalid_lock_toml);
//     EXPECT_FALSE(result.has_value());
// }

// TEST_F(ValidateMuukLockTomlTest, InvalidNestedDependencies) {
//     toml::value invalid_lock_toml = toml::parse(R"(
//         [[library]]
//         name = "example_lib"
//         version = "1.2.3"
//         dependencies = "should_be_array"
//     )");

//     auto result = muuk::validate_muuk_lock_toml(invalid_lock_toml);
//     EXPECT_FALSE(result.has_value());
// }

class DependencyNameTest : public ::testing::Test { };

TEST_F(DependencyNameTest, ValidNames) {
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("valid-name"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("package_name"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("pkg-name"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("gtkmm-4.0")); // Digits wrapping `.`
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("ncurses++")); // Double `+` allowed
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("pkg/name")); // Single `/` allowed
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("libboost_1.76"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("test-1.2.3"));
}

TEST_F(DependencyNameTest, InvalidNames) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("")); // Empty name
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("-start")); // Starts with `-`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("_start")); // Starts with `_`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("/start")); // Starts with `/`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("+start")); // Starts with `+`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("end-")); // Ends with `-`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("end_")); // Ends with `_`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("end/")); // Ends with `/`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("end++-")); // Ends with `++-`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a.b.c")); // `.` not wrapped by digits
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a..b")); // Consecutive `.`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a--b")); // Consecutive `-`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a__b")); // Consecutive `_`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a//b")); // Consecutive `/`
}

TEST_F(DependencyNameTest, InvalidSpecialCharacters) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg@name")); // `@` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg#name")); // `#` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg$name")); // `$` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg%name")); // `%` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg^name")); // `^` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg&name")); // `&` is invalid
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg*name")); // `*` is invalid
}

TEST_F(DependencyNameTest, ValidEdgeCases) {
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("a1")); // Smallest valid name
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("a-b")); // Single `-`
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("a_b")); // Single `_`
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("a/b")); // Single `/`
    // EXPECT_TRUE(muuk::validation::is_valid_dependency_name("a.1"));       // Digit-wrapped `.`
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("1.2.3")); // Version style valid
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("libc++")); // Exactly two `+`
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("ncurses++")); // Consecutive `+`
}

TEST_F(DependencyNameTest, InvalidPlusSigns) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg+name")); // Only one `+`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg+++name")); // More than two `+`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg+a+b")); // Non-consecutive `+`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg+1+")); // `+` not consecutive
}

TEST_F(DependencyNameTest, InvalidSlash) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg/name/test")); // More than one `/`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("/pkg/name")); // Starts with `/`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("pkg/name/")); // Ends with `/`
}

TEST_F(DependencyNameTest, ValidLongNames) {
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("super-long-package-name-1.2.3"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("valid_name_with_under_scores"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("complex-pkg_1.0.0-alpha"));
}

TEST_F(DependencyNameTest, InvalidLongNames) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("invalid__pkg_name")); // Double `_`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("invalid--pkg-name")); // Double `-`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("invalid..pkg.name")); // Double `.`
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("invalid/name/with/multiple/slashes"));
}

TEST_F(DependencyNameTest, InvalidEmptyOrSpaces) {
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name(" ")); // Single space
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name(" a")); // Leading space
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a ")); // Trailing space
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("a b")); // Space in the middle
    EXPECT_FALSE(muuk::validation::is_valid_dependency_name("\tname")); // Tab in name
}

TEST_F(DependencyNameTest, ValidMixedCases) {
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("test-lib_1.0.0"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("pkg/boost_1.82"));
    EXPECT_TRUE(muuk::validation::is_valid_dependency_name("foo-bar_2.1.4"));
}

class ValidateFlagTest : public ::testing::Test { };

TEST_F(ValidateFlagTest, ValidMSVCFlags) {
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/O2").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/W3").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/EHsc").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/std:c++20").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/MD").has_value());
}

TEST_F(ValidateFlagTest, InvalidMSVCFlags) {
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "").has_value()); // Empty
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "\\O2").has_value()); // Wrong prefix
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "O2").has_value()); // Missing `/`
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/flag!").has_value()); // Invalid char
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/Werror$").has_value()); // Invalid char
}

TEST_F(ValidateFlagTest, ValidGCCClangFlags) {
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-O2").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-Wall").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-Werror").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-std=c++17").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-DDEBUG").has_value());
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-march=native").has_value());
}

TEST_F(ValidateFlagTest, InvalidGCCClangFlags) {
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "").has_value()); // Empty
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "/O2").has_value()); // Wrong prefix
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "O2").has_value()); // Missing `-`
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-flag!").has_value()); // Invalid char
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-Wno$errors").has_value()); // Invalid char
}

TEST_F(ValidateFlagTest, EdgeCases) {
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "/D").has_value()); // Smallest valid flag (MSVC)
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-D").has_value()); // Smallest valid flag (GCC)
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-fno-exceptions").has_value()); // Long flag
    EXPECT_TRUE(muuk::validation::validate_flag(muuk::Compiler::GCC, "-march=x86-64").has_value()); // `-` inside flag
}

TEST_F(ValidateFlagTest, OnlyPrefixInvalid) {
    // EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::MSVC, "-").has_value()); // Only `/`
    EXPECT_FALSE(muuk::validation::validate_flag(muuk::Compiler::GCC, "/").has_value()); // Only `-`
}