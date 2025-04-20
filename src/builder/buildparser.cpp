#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <toml.hpp>

#include "buildconfig.h"
#include "buildmanager.h"
#include "buildparser.hpp"
#include "logger.h"
#include "moduleresolver.hpp"
#include "muuk.h"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "util.h"

namespace fs = std::filesystem;

Result<BuildProfile> extract_profile_flags(const std::string& profile, muuk::Compiler compiler, const toml::value& muuk_file) {
    muuk::logger::info("Extracting profile-specific flags for profile '{}'", profile);

    if (!muuk_file.contains("profile"))
        return Err("No 'profile' section found in configuration.");

    auto profile_table = muuk_file.at("profile").as_table();

    if (!profile_table.contains(profile))
        return Err("Profile '{}' does not exist in the configuration.", profile);

    auto profile_entry = profile_table.at(profile).as_table();

    BuildProfile build_profile;
    build_profile.cflags = muuk::parse_array_as_vec(profile_entry, "cflags");
    build_profile.lflags = muuk::parse_array_as_vec(profile_entry, "lflags");
    build_profile.defines = muuk::parse_array_as_vec(profile_entry, "defines", "-D");

    muuk::normalize_flags_inplace(build_profile.cflags, compiler);
    muuk::normalize_flags_inplace(build_profile.lflags, compiler);
    muuk::normalize_flags_inplace(build_profile.defines, compiler);

    muuk::logger::trace("Profile '{}' CFLAGS: {}", profile, fmt::join(build_profile.cflags, " "));
    muuk::logger::trace("Profile '{}' LFLAGS: {}", profile, fmt::join(build_profile.lflags, " "));
    muuk::logger::trace("Profile '{}' Defines: {}", profile, fmt::join(build_profile.defines, " "));

    return build_profile;
}

// Extract platform-specific flags
std::vector<std::string> extract_platform_flags(const toml::table& package_table) {
    std::vector<std::string> flags;
    if (!package_table.contains("platform"))
        return flags;

    auto platform_table = package_table.at("platform").as_table();

    std::string detected_platform;

#ifdef _WIN32
    detected_platform = "windows";
#elif __APPLE__
    detected_platform = "macos";
#elif __linux__
    detected_platform = "linux";
#else
    detected_platform = "unknown";
#endif

    if (platform_table.contains(detected_platform)) {
        auto platform_entry = platform_table.at(detected_platform).as_table();

        flags = muuk::parse_array_as_vec(platform_entry, "cflags");
    }

    return flags;
}

// Extract compiler-specific flags
std::vector<std::string> extract_compiler_flags(const toml::table& package_table, const muuk::Compiler compiler) {
    std::vector<std::string> flags;
    if (!package_table.contains("compiler"))
        return flags;

    auto compiler_table = package_table.at("compiler").as_table();

    auto compiler_ = compiler.to_string();

    if (compiler_table.contains(compiler_)) {
        auto compiler_entry = compiler_table.at(compiler_).as_table();

        flags = muuk::parse_array_as_vec(compiler_entry, "cflags");
    }

    return flags;
}

void parse_compilation_unit(BuildManager& build_manager, muuk::Compiler compiler, const toml::array& unit_array, const std::string& name, const std::filesystem::path& pkg_dir, const std::vector<std::string>& base_cflags, const std::vector<std::string>& platform_cflags, const std::vector<std::string>& compiler_cflags, const std::vector<std::string>& iflags) {

    for (const auto& unit_entry : unit_array) {
        if (!unit_entry.is_table())
            continue;

        if (!unit_entry.contains("path"))
            continue;

        std::string src_path = util::to_linux_path(
            std::filesystem::absolute(
                std::filesystem::path(unit_entry.at("path").as_string()))
                .string(),
            "../../");

        std::string obj_path = util::to_linux_path(
            (pkg_dir / std::filesystem::path(src_path).stem()).string() + OBJ_EXT,
            "../../");

        // Merge flags
        std::vector<std::string> unit_cflags = muuk::parse_array_as_vec(unit_entry, "cflags");
        unit_cflags.insert(unit_cflags.end(), base_cflags.begin(), base_cflags.end());
        unit_cflags.insert(unit_cflags.end(), platform_cflags.begin(), platform_cflags.end());
        unit_cflags.insert(unit_cflags.end(), compiler_cflags.begin(), compiler_cflags.end());

        muuk::normalize_flags_inplace(unit_cflags, compiler);

        // Register in build manager
        build_manager.add_compilation_target(src_path, obj_path, unit_cflags, iflags);

        // Logging
        muuk::logger::info("Added {} compilation target: {} -> {}", name, src_path, obj_path);
        muuk::logger::trace("  - {} CFLAGS: {}", name, fmt::join(unit_cflags, ", "));
        muuk::logger::trace("  - {} Include Flags: {}", name, fmt::join(iflags, ", "));
    }
}

void parse_compilation_targets(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const toml::value& muuk_file, const std::string& profile) {
    for (const std::string& name : { "build", "library" }) {
        if (!muuk_file.contains(name))
            continue;

        auto section = muuk_file.at(name).as_array();
        for (const auto& package : section) {
            const auto package_table = package.as_table();
            const auto package_name = package_table.at("name").as_string();
            const auto package_version = package_table.at("version").as_string();

            // Skip if 'profiles' is present and doesn't match
            if (name == "build" && package_table.contains("profiles")) {
                bool matched = false;
                for (const auto& p : package_table.at("profiles").as_array())
                    if (p.as_string() == profile) {
                        matched = true;
                        break;
                    }

                if (!matched)
                    continue;
            }

            const fs::path pkg_dir = (name == "build")
                ? build_dir / package_name
                : build_dir / package_name / package_version;

            std::filesystem::create_directories(pkg_dir);

            // Common flags
            auto cflags = muuk::parse_array_as_vec(package_table, "cflags");
            auto iflags = muuk::parse_array_as_vec(package_table, "include", "-I../../");
            auto defines = muuk::parse_array_as_vec(package_table, "defines", "-D");

            auto platform_cflags = extract_platform_flags(package_table);
            auto compiler_cflags = extract_compiler_flags(package_table, compiler);

            muuk::normalize_flags_inplace(cflags, compiler);
            muuk::normalize_flags_inplace(iflags, compiler);
            muuk::normalize_flags_inplace(defines, compiler);

            muuk::normalize_flags_inplace(platform_cflags, compiler);
            muuk::normalize_flags_inplace(compiler_cflags, compiler);

            // Parse Modules
            if (package_table.contains("modules")) {
                auto modules = package_table.at("modules").as_array();
                parse_compilation_unit(build_manager, compiler, modules, "module", pkg_dir, cflags, platform_cflags, compiler_cflags, iflags);
            }

            // Parse Sources
            if (package_table.contains("sources")) {
                auto sources = package_table.at("sources").as_array();
                parse_compilation_unit(build_manager, compiler, sources, "source", pkg_dir, cflags, platform_cflags, compiler_cflags, iflags);
            }
        }
    }

    muuk::resolve_modules(build_manager, build_dir.string());
};

void parse_libraries(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const toml::value& muuk_file) {

    for (const auto& library_table : muuk_file.at("library").as_array()) {
        const auto library_name = library_table.at("name").as_string();
        const auto library_version = library_table.at("version").as_string();
        const auto lib_path = util::to_linux_path(
            (build_dir / library_name / library_version / (library_name + LIB_EXT)).string(),
            "../../");

        std::vector<std::string> obj_files;
        if (library_table.contains("sources")) {
            auto sources = library_table.at("sources").as_array();
            for (const auto& src_entry : sources) {
                if (!src_entry.is_table())
                    continue;
                auto source_table = src_entry.as_table();
                if (!source_table.contains("path"))
                    continue;

                const auto obj_file = util::to_linux_path(
                    (build_dir / library_name / library_version / std::filesystem::path(source_table.at("path").as_string()).stem()).string() + OBJ_EXT,
                    "../../");
                obj_files.push_back(obj_file);
            }
        }

        auto aflags = muuk::parse_array_as_vec(library_table.as_table(), "aflags");
        muuk::normalize_flags_inplace(aflags, compiler);
        build_manager.add_archive_target(lib_path, obj_files, aflags);

        muuk::logger::info("Added library target: {}", lib_path);
        muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
        muuk::logger::trace("  - Archive Flags: {}", fmt::join(aflags, ", "));
    }
};

void parse_executables(
    BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const std::string& profile_, const toml::value& muuk_file) {
    if (!muuk_file.contains("build") || !muuk_file.contains("library"))
        return;

    const auto& build_sections = muuk_file.at("build").as_array();
    const auto& library_sections = muuk_file.at("library").as_array();

    // Index libraries by name + version
    std::unordered_map<std::string, std::unordered_map<std::string, toml::value>> lib_map;
    for (const auto& lib : library_sections) {
        const auto& lib_table = lib.as_table();
        const auto& name = lib_table.at("name").as_string();
        const auto& version = lib_table.at("version").as_string();
        lib_map[name][version] = lib;
    }

    for (const auto& entry : build_sections) {
        const auto& build_table = entry.as_table();
        const std::string executable_name = build_table.at("name").as_string();

        // Profile matching
        if (build_table.contains("profiles")) {
            bool matched = false;
            for (const auto& profile : build_table.at("profiles").as_array()) {
                if (profile.as_string() == profile_) {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                continue;
        }

        // Prepare build paths
        const auto exe_path = util::to_linux_path(
            (build_dir / executable_name / (executable_name + EXE_EXT)).string(), "../../");

        std::vector<std::string> obj_files;
        std::vector<std::string> libs;
        std::vector<std::string> iflags;

        muuk::logger::info(fmt::format("Parsing executable '{}'", executable_name));

        // Source → Object files
        if (build_table.contains("sources")) {
            for (const auto& src : build_table.at("sources").as_array()) {
                if (!src.is_table())
                    continue;
                const auto& src_table = src.as_table();
                if (!src_table.contains("path"))
                    continue;

                std::string src_path = util::to_linux_path(
                    std::filesystem::absolute(std::filesystem::path(src_table.at("path").as_string())).string(),
                    "../../");

                std::string obj_path = util::to_linux_path(
                    (build_dir / executable_name / std::filesystem::path(src_path).stem()).string() + OBJ_EXT,
                    "../../");

                obj_files.push_back(obj_path);
            }
        }

        // Dependencies
        if (build_table.contains("dependencies")) {
            for (const auto& dep : build_table.at("dependencies").as_array()) {
                const auto& dep_table = dep.as_table();
                const auto& lib_name = dep_table.at("name").as_string();
                const auto& version = dep_table.at("version").as_string();

                if (lib_map.contains(lib_name) && lib_map.at(lib_name).contains(version)) {
                    const auto& lib_table = lib_map.at(lib_name).at(version).as_table();

                    if (lib_table.contains("sources")) {
                        std::string lib_path = util::to_linux_path(
                            (build_dir / lib_name / version / (lib_name + LIB_EXT)).string(), "../../");
                        libs.push_back(lib_path);
                    }

                    if (lib_table.contains("include")) {
                        for (const auto& inc : lib_table.at("include").as_array()) {
                            iflags.push_back("-I../../" + inc.as_string());
                        }
                    }
                }
            }
        }

        // Linker flags
        std::vector<std::string> lflags = muuk::parse_array_as_vec(build_table, "lflags");

        muuk::normalize_flags_inplace(lflags, compiler);

        // Register build
        build_manager.add_link_target(exe_path, obj_files, libs, lflags);

        muuk::logger::info("Added link target: {}", exe_path);
        muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
        muuk::logger::trace("  - Libraries: {}", fmt::join(libs, ", "));
        muuk::logger::trace("  - Include Flags: {}", fmt::join(iflags, ", "));
        muuk::logger::trace("  - Linker Flags: {}", fmt::join(lflags, ", "));
    }
}

Result<void> parse(BuildManager& build_manager, muuk::Compiler compiler, const std::filesystem::path& build_dir, const std::string& profile) {

    auto result = muuk::parse_muuk_file("build/muuk.lock.toml", true);
    if (!result) {
        return Err(result);
    }
    auto muuk_file = result.value();

    auto profile_result = extract_profile_flags(profile, compiler, muuk_file);
    if (!profile_result) {
        return Err(profile_result);
    }

    build_manager.set_profile_flags(profile, profile_result.value());

    parse_compilation_targets(build_manager, compiler, build_dir, muuk_file, profile);
    parse_libraries(build_manager, compiler, build_dir, muuk_file);
    parse_executables(build_manager, compiler, build_dir, profile, muuk_file);

    return {};
}

std::pair<std::string, std::string> get_profile_flag_strings(BuildManager& manager, const std::string& profile) {
    const auto* build_profile = manager.get_profile(profile);
    std::string profile_cflags, profile_lflags;

    if (build_profile) {
        std::ostringstream cflags_stream, lflags_stream;

        for (const auto& flag : build_profile->cflags)
            cflags_stream << flag << " ";
        for (const auto& def : build_profile->defines)
            cflags_stream << def << " ";
        for (const auto& flag : build_profile->lflags)
            lflags_stream << flag << " ";

        profile_cflags = cflags_stream.str();
        profile_lflags = lflags_stream.str();
    } else {
        muuk::logger::warn("No profile flags found for '{}'", profile);
    }

    return { profile_cflags, profile_lflags };
}
