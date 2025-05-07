#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <toml.hpp>

#include "build/manager.hpp"
#include "build/module_resolver.hpp"
#include "build/parser.hpp"
#include "build/targets.hpp"
#include "buildconfig.h"
#include "logger.hpp"
#include "muuk.hpp"
#include "muuk_parser.hpp"
#include "rustify.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

static constexpr std::string_view to_string(CompilationUnitType value) {
    constexpr std::array<std::string_view, static_cast<size_t>(CompilationUnitType::Count)> names = {
        "module", "source"
    };
    return names.at(static_cast<size_t>(value));
}

static Result<BuildProfile> extract_profile_flags(const std::string& profile, const muuk::Compiler compiler, const toml::value& muuk_file) {
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

/// Generic flag extractor from a TOML table given a section and a key
std::vector<std::string> extract_flags_by_key(
    const toml::table& package_table,
    const std::string& section,
    const std::string& key_name) {
    std::vector<std::string> flags;

    if (!package_table.contains(section))
        return flags;

    auto section_table = package_table.at(section).as_table();

    if (section_table.contains(key_name)) {
        auto entry = section_table.at(key_name).as_table();
        flags = muuk::parse_array_as_vec(entry, "cflags");
    }

    return flags;
}

/// Extract platform-specific flags
std::vector<std::string> extract_platform_flags(const toml::table& package_table) {
    std::string detected_platform;

#ifdef _WIN32
    detected_platform = "windows";
#elif __APPLE__
    detected_platform = "macos";
#elif __linux__
    detected_platform = "linux";
#else
    detected_platform = "unknown"; // Fallback for unsupported platforms
#endif

    return extract_flags_by_key(package_table, "platform", detected_platform);
}

/// Extract compiler-specific flags
std::vector<std::string> extract_compiler_flags(const toml::table& package_table, const muuk::Compiler compiler) {
    return extract_flags_by_key(package_table, "compiler", compiler.to_string());
}

inline std::pair<std::string, std::string> get_src_and_obj_paths(
    const toml::value& unit_entry,
    const fs::path& build_dir) {

    const std::string raw_path = unit_entry.at("path").as_string();

    const std::string src_path = util::file_system::to_linux_path(
        std::filesystem::absolute(
            std::filesystem::path(raw_path))
            .string());

    const std::string obj_path = util::file_system::sanitize_path(
        util::file_system::to_linux_path(
            (build_dir.string() + "/" + raw_path + OBJ_EXT),
            "../../"));

    return { src_path, obj_path };
}

/// Parses compilation units (modules or sources) from the TOML array
void parse_compilation_unit(BuildManager& build_manager, const toml::array& unit_array, const CompilationUnitType compilation_unit_type, const std::filesystem::path& build_dir, const CompilationFlags compilation_flags) {
    for (const auto& unit_entry : unit_array) {
        if (!unit_entry.is_table())
            continue;

        if (!unit_entry.contains("path"))
            continue;

        const auto [src_path, obj_path] = get_src_and_obj_paths(unit_entry, build_dir);

        // Register in build manager
        build_manager.add_compilation_target(
            src_path,
            obj_path,
            compilation_flags,
            compilation_unit_type);

        // Logging
        muuk::logger::info("Added {} compilation target: {} -> {}", to_string(compilation_unit_type), src_path, obj_path);
        muuk::logger::trace("  - CFlags: {}", fmt::join(compilation_flags.cflags, ", "));
        muuk::logger::trace("  - Defines: {}", fmt::join(compilation_flags.defines, ", "));
        muuk::logger::trace("  - Include Flags: {}", fmt::join(compilation_flags.iflags, ", "));
        muuk::logger::trace("  - Platform CFlags: {}", fmt::join(compilation_flags.platform_cflags, ", "));
        muuk::logger::trace("  - Compiler CFlags: {}", fmt::join(compilation_flags.compiler_cflags, ", "));
    }
}

void parse_compilation_targets(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const toml::value& muuk_file, const std::string& profile) {
    bool has_modules = false;

    for (const std::string& name : { "build", "library" }) {
        if (!muuk_file.contains(name))
            continue;

        auto section = muuk_file.at(name).as_array();
        for (const auto& package : section) {
            const auto package_table = package.as_table();
            const auto package_name = package_table.at("name").as_string();
            const auto package_version = package_table.at("version").as_string();

            // Skip if 'profiles' is present and doesn't match
            if (package_table.contains("profiles")) {
                bool matched = false;
                for (const auto& p : package_table.at("profiles").as_array())
                    if (p.as_string() == profile) {
                        matched = true;
                        break;
                    }

                if (!matched)
                    continue;
            }

            // Common flags
            auto cflags = muuk::parse_array_as_vec(package_table, "cflags");
            auto iflags = muuk::parse_array_as_vec(package_table, "include", "-I../../");
            auto defines = muuk::parse_array_as_vec(package_table, "defines", "-D");

            // Extract platform-specific and compiler-specific flags
            auto platform_cflags = extract_platform_flags(package_table);
            auto compiler_cflags = extract_compiler_flags(package_table, compiler);

            muuk::normalize_flags_inplace(cflags, compiler);
            muuk::normalize_flags_inplace(iflags, compiler);
            muuk::normalize_flags_inplace(defines, compiler);

            muuk::normalize_flags_inplace(platform_cflags, compiler);
            muuk::normalize_flags_inplace(compiler_cflags, compiler);

            CompilationFlags compilation_flags = {
                cflags,
                iflags,
                defines,
                platform_cflags,
                compiler_cflags
            };

            // Parse Modules
            if (package_table.contains("modules")) {
                auto modules = package_table.at("modules").as_array();
                parse_compilation_unit(
                    build_manager,
                    modules,
                    CompilationUnitType::Module,
                    build_dir,
                    compilation_flags);
                has_modules = true;
            }

            // Parse Sources
            if (package_table.contains("sources")) {
                auto sources = package_table.at("sources").as_array();
                parse_compilation_unit(
                    build_manager,
                    sources,
                    CompilationUnitType::Source,
                    build_dir,
                    compilation_flags);
            }
        }
    }
    if (has_modules)
        muuk::resolve_modules(build_manager, build_dir.string());
};

void parse_libraries(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const toml::value& muuk_file, const std::string& profile) {
    for (const auto& library_table : muuk_file.at("library").as_array()) {
        const auto library_name = library_table.at("name").as_string();
        const auto library_version = library_table.at("version").as_string();
        const auto lib_path_dir = (build_dir / library_table.at("path").as_string()).lexically_normal();

        const auto lib_path = util::file_system::to_linux_path(
            (lib_path_dir / (library_name + LIB_EXT)).string(),
            "../../");

        // Skip if 'profiles' is present and doesn't match
        if (library_table.contains("profiles")) {
            bool matched = false;
            for (const auto& p : library_table.at("profiles").as_array())
                if (p.as_string() == profile) {
                    matched = true;
                    break;
                }

            if (!matched)
                continue;
        }

        std::vector<std::string> obj_files;

        auto parse_entries = [&build_dir, &obj_files](const toml::array& entries) {
            for (const auto& entry : entries) {
                if (!entry.is_table())
                    continue;
                auto entry_table = entry.as_table();
                if (!entry_table.contains("path"))
                    continue;

                const auto [_, obj_path] = get_src_and_obj_paths(entry_table, build_dir);

                obj_files.push_back(obj_path);
            }
        };

        // Parse modules
        if (library_table.contains("modules")) {
            parse_entries(library_table.at("modules").as_array());
        }

        // Parse sources
        if (library_table.contains("sources")) {
            parse_entries(library_table.at("sources").as_array());
        }

        auto aflags = muuk::parse_array_as_vec(library_table.as_table(), "aflags");
        muuk::normalize_flags_inplace(aflags, compiler);
        build_manager.add_archive_target(lib_path, obj_files, aflags);

        muuk::logger::info("Added library target: {}", lib_path);
        muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
        muuk::logger::trace("  - Archive Flags: {}", fmt::join(aflags, ", "));
    }
};

Result<std::vector<ExternalTarget>> parse_external_targets(const toml::value& muuk_file) {
    std::vector<ExternalTarget> externals;

    if (!muuk_file.contains("external"))
        return externals;

    const auto& ext_array = muuk_file.at("external").as_array();

    for (const auto& entry : ext_array) {
        if (!entry.is_table())
            continue;

        const auto& ext_table = entry.as_table();

        try {
            const std::string name = ext_table.at("name").as_string();
            const std::string version = ext_table.at("version").as_string();
            const std::string type = ext_table.at("type").as_string();
            const std::string path = ext_table.at("path").as_string();

            std::vector<std::string> args;
            if (ext_table.contains("args")) {
                for (const auto& arg : ext_table.at("args").as_array()) {
                    args.push_back(arg.as_string());
                }
            }

            std::vector<std::string> outputs;
            if (ext_table.contains("outputs")) {
                for (const auto& out : ext_table.at("outputs").as_array())
                    outputs.push_back(out.as_string());

            } else
                return Err("Missing 'outputs' field for external target '{}'", name);

            externals.emplace_back(name, version, type, path, args, outputs);
            muuk::logger::info("Parsed external target '{}'", name);

        } catch (const std::exception& e) {
            return Err("Failed to parse [external] entry: {}", e.what());
        }
    }

    return externals;
}

void parse_executables(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const std::filesystem::path& build_artifact_dir, const std::string& profile_, const toml::value& muuk_file) {
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
        const auto exe_path = util::file_system::to_linux_path(
            (build_dir / (executable_name + EXE_EXT)).string(), "../../");

        std::vector<std::string> obj_files;
        std::vector<std::string> libs;

        muuk::logger::info(fmt::format("Parsing executable '{}'", executable_name));

        // Source → Object files
        if (build_table.contains("sources")) {
            for (const auto& src : build_table.at("sources").as_array()) {
                if (!src.is_table())
                    continue;
                const auto& src_table = src.as_table();
                if (!src_table.contains("path"))
                    continue;

                const auto [_, obj_path] = get_src_and_obj_paths(src_table, build_artifact_dir);

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

                    const auto lib_path_dir = (build_artifact_dir / lib_table.at("path").as_string()).lexically_normal();

                    // If it doesn't have these keys, then its an empty library so we should skip it
                    if (lib_table.contains("sources") || lib_table.contains("modules")) {
                        const auto lib_path = util::file_system::to_linux_path(
                            (lib_path_dir / (lib_name + LIB_EXT)).string(), "../../");
                        libs.push_back(lib_path);
                    }
                }
            }
        }

        if (build_table.contains("libs")) {
            for (const auto& lib : build_table.at("libs").as_array()) {
                if (!lib.is_string())
                    continue;
                libs.push_back(lib.as_string());
            }
        }

        // Linker flags
        std::vector<std::string> lflags = muuk::parse_array_as_vec(build_table, "lflags");

        muuk::normalize_flags_inplace(lflags, compiler);

        // Register build
        build_manager.add_link_target(exe_path, obj_files, libs, lflags);

        muuk::logger::info("Added link target: {}", exe_path);
        muuk::logger::trace("  - Object Files: '{}'", fmt::join(obj_files, "', '"));
        muuk::logger::trace("  - Libraries: '{}'", fmt::join(libs, "', '"));
        muuk::logger::trace("  - Linker Flags: '{}'", fmt::join(lflags, "', '"));
    }
}

Result<void> parse(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const std::string& profile) {
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

    const auto build_artifact_dir = build_dir / MUUK_FILES;
    util::file_system::ensure_directory_exists(build_artifact_dir.string());

    parse_compilation_targets(
        build_manager,
        compiler,
        build_artifact_dir,
        muuk_file,
        profile);
    parse_libraries(
        build_manager,
        compiler,
        build_artifact_dir,
        muuk_file,
        profile);
    parse_executables(
        build_manager,
        compiler,
        build_dir,
        build_artifact_dir,
        profile,
        muuk_file);

    return {};
}

std::pair<std::string, std::string> get_profile_flag_strings(const BuildManager& manager, const std::string& profile) {
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
