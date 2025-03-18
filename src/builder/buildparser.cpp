#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <toml++/toml.hpp>

#include "buildconfig.h"
#include "buildmanager.h"
#include "buildparser.hpp"
#include "logger.h"
#include "moduleresolver.hpp"
#include "muuk.h"
#include "muukfiler.h"
#include "util.h"

namespace fs = std::filesystem;

BuildParser::BuildParser(std::shared_ptr<BuildManager> manager, std::shared_ptr<MuukFiler> muuk_filer, muuk::Compiler compiler, const fs::path& build_dir, std::string profile) :
    build_manager(std::move(manager)),
    muuk_filer(std::move(muuk_filer)),
    build_dir(build_dir),
    compiler(compiler),
    profile_(profile) {
}

void BuildParser::parse() {
    parse_compilation_targets();
    parse_libraries();
    parse_executables();
}

void BuildParser::parse_compilation_targets() {
    const auto& config_sections = muuk_filer->get_array_section();

    // const auto& build_sections = muuk_filer->get_build_section();
    // const auto& library_sections = muuk_filer->get_library_section();

    // if (build_sections.empty()) {
    //     muuk::logger::error("No build sections found.");
    //     return;
    // }

    if (config_sections.empty()) {
        muuk::logger::error("No configuration sections found.");
        return;
    }

    if (config_sections.find("build") == config_sections.end()) {
        muuk::logger::error("No 'build' section found in `lockfile`.");
        return;
    }

    if (config_sections.find("library") == config_sections.end()) {
        muuk::logger::warn("No 'library' section found in `lockfile`.");
        return;
    }

    for (std::string name : { "build", "library" }) {
        auto section = config_sections.at(name);

        for (const auto& package_table : section) {

            // This can be removed eventually
            // Once my validator gurantees that these keys exist
            if (!package_table.contains("name") || !package_table.at("name").is_string()) {
                muuk::logger::error("Package name not found in section: {}", name);
                continue;
            }

            if (!package_table.contains("version") || !package_table.at("version").is_string()) {
                muuk::logger::error("Package version not found in section: {}", name);
                continue;
            }

            const auto package_name = package_table.at("name").value<std::string>().value();
            const auto package_version = package_table.at("version").value<std::string>().value();
            // const fs::path pkg_dir = build_dir / package_name / package_version;

            fs::path pkg_dir;
            if (std::string(name) == "build") {
                pkg_dir = build_dir / package_name;
            } else {
                pkg_dir = build_dir / package_name / package_version;
            }

            // Ninja creates this I believe
            // TODO: only create if theres anything that will be put there
            std::filesystem::create_directories(pkg_dir);

            std::vector<std::string> cflags = MuukFiler::parse_array_as_vec(package_table, "cflags");
            std::vector<std::string> iflags = MuukFiler::parse_array_as_vec(package_table, "include", "-I../../");
            std::vector<std::string> defines = MuukFiler::parse_array_as_vec(package_table, "defines", "-D");

            // Extract platform and compiler-specific flags
            std::vector<std::string> platform_cflags = extract_platform_flags(package_table);
            std::vector<std::string> compiler_cflags = extract_compiler_flags(package_table);

            muuk::normalize_flags_inplace(cflags, compiler);
            muuk::normalize_flags_inplace(iflags, compiler);
            muuk::normalize_flags_inplace(platform_cflags, compiler);
            muuk::normalize_flags_inplace(compiler_cflags, compiler);

            if (package_table.contains("modules")) {
                auto modules = package_table.at("modules").as_array();
                if (modules) {
                    for (const auto& mod_entry : *modules) {
                        if (!mod_entry.is_table())
                            continue;

                        auto module_table = mod_entry.as_table();
                        if (!module_table->contains("path"))
                            continue;

                        std::string mod_path = util::to_linux_path(
                            std::filesystem::absolute(
                                std::filesystem::path(*module_table->at("path").value<std::string>()))
                                .string(),
                            "../../");

                        std::string mod_obj_path = util::to_linux_path(
                            (pkg_dir / "modules" / std::filesystem::path(mod_path).stem()).string() + OBJ_EXT,
                            "../../");

                        std::vector<std::string> mod_cflags = MuukFiler::parse_array_as_vec(*module_table, "cflags");
                        mod_cflags.insert(mod_cflags.end(), cflags.begin(), cflags.end());
                        mod_cflags.insert(mod_cflags.end(), platform_cflags.begin(), platform_cflags.end());
                        mod_cflags.insert(mod_cflags.end(), compiler_cflags.begin(), compiler_cflags.end());

                        muuk::normalize_flags_inplace(mod_cflags, compiler);

                        // Add module as a compilation target
                        build_manager->add_compilation_target(mod_path, mod_obj_path, mod_cflags, iflags);

                        muuk::logger::info("Added module compilation target: {} -> {}", mod_path, mod_obj_path);
                        muuk::logger::trace("  - Module CFLAGS: {}", fmt::join(mod_cflags, ", "));
                        muuk::logger::trace("  - Module Include Flags: {}", fmt::join(iflags, ", "));
                    }
                }
            }

            if (package_table.contains("sources")) {
                auto sources = package_table.at("sources").as_array();
                if (sources) {
                    for (const auto& src_entry : *sources) {
                        if (!src_entry.is_table())
                            continue;
                        auto source_table = src_entry.as_table();
                        if (!source_table->contains("path"))
                            continue;

                        std::string src_path = util::to_linux_path(
                            std::filesystem::absolute(
                                std::filesystem::path(
                                    *source_table->at("path").value<std::string>()))
                                .string(),
                            "../../");

                        std::string obj_path = util::to_linux_path(
                            (pkg_dir / std::filesystem::path(src_path).stem()).string()
                                + OBJ_EXT,
                            "../../");

                        std::vector<std::string> src_cflags = MuukFiler::parse_array_as_vec(*source_table, "cflags");
                        src_cflags.insert(src_cflags.end(), cflags.begin(), cflags.end());

                        // Add platform and compiler-specific flags
                        src_cflags.insert(src_cflags.end(), platform_cflags.begin(), platform_cflags.end());
                        src_cflags.insert(src_cflags.end(), compiler_cflags.begin(), compiler_cflags.end());

                        muuk::normalize_flags_inplace(src_cflags, compiler);

                        build_manager->add_compilation_target(src_path, obj_path, src_cflags, iflags);

                        muuk::logger::info("Added compilation target: {} -> {}", src_path, obj_path);
                        muuk::logger::trace("  - CFLAGS: {}", fmt::join(src_cflags, ", "));
                        muuk::logger::trace("  - Include Flags: {}", fmt::join(iflags, ", "));
                    }
                }
            }
        }
    }

    muuk::resolve_modules(build_manager, build_dir.string());
}

void BuildParser::parse_libraries() {
    const auto& library_sections = muuk_filer->get_library_section();

    for (const auto& [library_name, versions] : library_sections) {
        for (const auto& [library_version, library_table] : versions) {
            std::string lib_path = util::to_linux_path(
                (build_dir / library_name / library_version / (library_name + LIB_EXT)).string(),
                "../../");

            std::vector<std::string> obj_files;
            if (library_table.contains("sources")) {
                auto sources = library_table.at("sources").as_array();
                if (sources) {
                    for (const auto& src_entry : *sources) {
                        if (!src_entry.is_table())
                            continue;
                        auto source_table = src_entry.as_table();
                        if (!source_table->contains("path"))
                            continue;

                        std::string obj_file = util::to_linux_path(
                            (build_dir / library_name / library_version / std::filesystem::path(*source_table->at("path").value<std::string>()).stem()).string() + OBJ_EXT,
                            "../../");
                        obj_files.push_back(obj_file);
                    }
                }
            }

            std::vector<std::string> aflags = MuukFiler::parse_array_as_vec(library_table, "aflags");
            muuk::normalize_flags_inplace(aflags, compiler);
            build_manager->add_archive_target(lib_path, obj_files, aflags);

            muuk::logger::info("Added library target: {}", lib_path);
            muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
            muuk::logger::trace("  - Archive Flags: {}", fmt::join(aflags, ", "));
        }
    }
}

void BuildParser::parse_executables() {
    const auto& build_sections = muuk_filer->get_build_section();
    const auto& library_sections = muuk_filer->get_library_section();

    for (const auto& [executable_name, build_table] : build_sections) {
        if (build_table.contains("profiles")) {
            auto profiles = build_table.at("profiles").as_array();
            if (profiles) {
                bool profile_found = false;
                for (const auto& profile : *profiles) {
                    if (profile.is_string() && profile.value<std::string>() == profile_) {
                        profile_found = true;
                        break;
                    }
                }
                if (!profile_found)
                    continue;
            }
        }

        std::string exe_path = util::to_linux_path(
            (build_dir / executable_name / (executable_name + EXE_EXT)).string(),
            "../../");
        std::vector<std::string> obj_files;
        std::vector<std::string> libs;
        std::vector<std::string> iflags; // Include flags for header-only libraries

        muuk::logger::info(fmt::format("Parsing executable '{}'", executable_name));

        // TODO: Once the validator work the array checks won't be needed
        // Collect all source files -> Convert to object files
        if (build_table.contains("sources") && build_table.at("sources").is_array()) {
            auto sources = build_table.at("sources").as_array();
            if (sources) {
                for (const auto& src_entry : *sources) {
                    if (!src_entry.is_table())
                        continue;

                    auto source_table = src_entry.as_table();
                    if (!source_table->contains("path"))
                        continue;

                    std::string src_path = util::to_linux_path(
                        std::filesystem::absolute(
                            std::filesystem::path(*source_table->at("path").value<std::string>()))
                            .string(),
                        "../../");

                    std::string obj_path = util::to_linux_path(
                        (build_dir / executable_name / std::filesystem::path(src_path).stem()).string()
                            + OBJ_EXT,
                        "../../");

                    obj_files.push_back(obj_path);
                }
            }
        }

        // EXES REQUIRE MERGED DEPENDENCIES
        // MODULES ONLY REQUIRE DIRECT DEPENDENCIES
        // Collect all dependencies (libraries & header-only includes)
        if (build_table.contains("dependencies2") && build_table.at("dependencies2").is_array()) {
            auto dependencies = build_table.at("dependencies2").as_array();
            if (dependencies) {
                for (const auto& dep : *dependencies) {
                    std::string library_name = dep.as_table()->at("name").value<std::string>().value();
                    std::string library_version = dep.as_table()->at("version").value<std::string>().value();

                    // Check if the library has sources (meaning it's a compiled library)
                    if (library_sections.contains(library_name) && library_sections.at(library_name).contains(library_version)) {
                        const auto& lib_table = library_sections.at(library_name).at(library_version);

                        if (lib_table.contains("sources")) {
                            // Library has sources, so we expect a .lib file
                            std::string lib_path = util::to_linux_path(
                                (build_dir / library_name / library_version / (library_name + LIB_EXT)).string(),
                                "../../");
                            libs.push_back(lib_path);
                        }

                        if (lib_table.contains("include")) {
                            // Header-only library, add include directories
                            std::vector<std::string> include_flags = MuukFiler::parse_array_as_vec(lib_table, "include", "-I../../");
                            iflags.insert(iflags.end(), include_flags.begin(), include_flags.end());
                        }
                    }
                }
            }
        }

        // Collect linker flags
        std::vector<std::string> lflags = MuukFiler::parse_array_as_vec(build_table, "lflags");

        muuk::normalize_flags_inplace(lflags, compiler);

        // Add to build manager
        build_manager->add_link_target(exe_path, obj_files, libs, lflags);

        muuk::logger::info("Added link target: {}", exe_path);
        muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
        muuk::logger::trace("  - Libraries: {}", fmt::join(libs, ", "));
        muuk::logger::trace("  - Include Flags: {}", fmt::join(iflags, ", "));
        muuk::logger::trace("  - Linker Flags: {}", fmt::join(lflags, ", "));
    }
}

/** Extract platform-specific FLAGS */
std::vector<std::string> BuildParser::extract_platform_flags(const toml::table& package_table) {
    std::vector<std::string> flags;
    if (!package_table.contains("platform"))
        return flags;

    auto platform_table = package_table.at("platform").as_table();
    if (!platform_table)
        return flags;

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

    if (platform_table->contains(detected_platform)) {
        auto platform_entry = platform_table->at(detected_platform).as_table();
        if (!platform_entry)
            return flags;

        flags = MuukFiler::parse_array_as_vec(*platform_entry, "cflags");

    } else {
        muuk::logger::warn("No configuration found for platform '{}'.", detected_platform);
    }

    return flags;
}

/** Extract compiler-specific FLAGS */
std::vector<std::string> BuildParser::extract_compiler_flags(const toml::table& package_table) {
    std::vector<std::string> flags;
    if (!package_table.contains("compiler"))
        return flags;

    auto compiler_table = package_table.at("compiler").as_table();
    if (!compiler_table)
        return flags;

    auto compiler_ = compiler.to_string();

    if (compiler_table->contains(compiler_)) {
        auto compiler_entry = compiler_table->at(compiler_).as_table();
        if (!compiler_entry)
            return flags;

        flags = MuukFiler::parse_array_as_vec(*compiler_entry, "cflags");
    } else {
        muuk::logger::warn("No configuration found for compiler '{}'.", compiler_);
    }

    return flags;
}

std::pair<std::string, std::string> BuildParser::extract_profile_flags(const std::string& profile) {
    muuk::logger::info("Extracting profile-specific flags for profile '{}'", profile);

    std::string profile_cflags_str, profile_lflags_str;

    // Fetch the parsed TOML configuration
    const auto& config = muuk_filer->get_config();

    if (!config.contains("profile")) {
        muuk::logger::warn("No 'profile' section found in configuration.");
        return { "", "" };
    }

    auto profile_table = config.at("profile").as_table();
    if (!profile_table) {
        muuk::logger::warn("No valid 'profile' table found.");
        return { "", "" };
    }

    if (!profile_table->contains(profile)) {
        muuk::logger::warn("Profile '{}' does not exist in the configuration.", profile);
        return { "", "" };
    }

    auto profile_entry = profile_table->at(profile).as_table();
    if (!profile_entry) {
        muuk::logger::warn("Profile '{}' is not properly defined.", profile);
        return { "", "" };
    }

    // Handle CFLAGS extraction safely
    std::vector<std::string> profile_cflags = MuukFiler::parse_array_as_vec(*profile_entry, "cflags");
    for (const auto& flag : profile_cflags) {
        profile_cflags_str += muuk::normalize_flag(flag, compiler) + " ";
    }
    muuk::logger::info("Profile '{}' CFLAGS: {}", profile, profile_cflags_str);

    // Handle LFLAGS extraction safely
    std::vector<std::string> profile_lflags = MuukFiler::parse_array_as_vec(*profile_entry, "lflags");
    for (const auto& flag : profile_lflags) {
        profile_lflags_str += muuk::normalize_flag(flag, compiler) + " ";
    }
    muuk::logger::info("Profile '{}' LFLAGS: {}", profile, profile_lflags_str);

    std::vector<std::string> profile_defines = MuukFiler::parse_array_as_vec(*profile_entry, "defines", "-D");
    for (const auto& flag : profile_defines) {
        profile_cflags_str += muuk::normalize_flag(flag, compiler) + " ";
    }
    muuk::logger::info("Profile '{}' Defines: {}", profile, profile_cflags_str);

    muuk::logger::info("Extracted profile_cflags: '{}'", profile_cflags_str);
    muuk::logger::info("Extracted profile_lflags: '{}'", profile_lflags_str);

    return { profile_cflags_str, profile_lflags_str };
}
