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
#include "compiler.hpp"
#include "logger.hpp"
#include "muuk.hpp"
#include "muuk_parser.hpp"
#include "opt_level.hpp"
#include "rustify.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace muuk {
    namespace build {
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

            auto profile_entry = profile_table.at(profile);

            BuildProfile build_profile;
            build_profile.cflags = muuk::parse_array_as_vec(profile_entry, "cflags");
            build_profile.aflags = muuk::parse_array_as_vec(profile_entry, "aflags");
            build_profile.lflags = muuk::parse_array_as_vec(profile_entry, "lflags");
            build_profile.defines = muuk::parse_array_as_vec(profile_entry, "defines", "-D");

            // --- Link Type Optimization ---
            if (profile_entry.at("lto").as_boolean()) {
                muuk::logger::info("LTO enabled for profile '{}'", profile);
                switch (compiler.getType()) {
                case Compiler::Type::GCC:
                case Compiler::Type::Clang:
                    build_profile.cflags.push_back("-flto");
                    build_profile.lflags.push_back("-flto");
                    break;
                case Compiler::Type::MSVC:
                    build_profile.cflags.push_back("/GL");
                    build_profile.lflags.push_back("/LTCG");
                    break;
                }
            }

            // --- Debug ---
            if (profile_entry.at("debug").as_boolean()) {
                muuk::logger::info("LTO enabled for profile '{}'", profile);
                switch (compiler.getType()) {
                case Compiler::Type::GCC:
                case Compiler::Type::Clang:
                    build_profile.cflags.push_back("-g");
                    break;
                case Compiler::Type::MSVC:
                    build_profile.cflags.push_back("/Zi");
                    build_profile.lflags.push_back("/DEBUG");
                    break;
                }
            }

            // --- Debug Assertions ---
            if (!profile_entry.at("debug-assertions").as_boolean()) {
                muuk::logger::info("LTO enabled for profile '{}'", profile);
                switch (compiler.getType()) {
                case Compiler::Type::GCC:
                case Compiler::Type::Clang:
                    build_profile.cflags.push_back("-DNDEBUG");
                    break;
                case Compiler::Type::MSVC:
                    build_profile.cflags.push_back("/DNDEBUG");
                    break;
                }
            }

            // --- Optimization Level ---
            build_profile.cflags.push_back(to_flag(
                opt_lvl_from_string(profile_entry.at("opt-level").as_string()),
                compiler.getType()));

            // --- Sanitizers ---
            if (profile_entry.contains("sanitizers") && profile_entry.at("sanitizers").is_array()) {
                for (const auto& item : profile_entry.at("sanitizers").as_array()) {
                    if (!item.is_string())
                        continue;

                    const std::string name = item.as_string();

                    switch (compiler.getType()) {
                    case Compiler::Type::GCC:
                    case Compiler::Type::Clang:
                        if (name == "address")
                            build_profile.cflags.push_back("-fsanitize=address");
                        else if (name == "thread")
                            build_profile.cflags.push_back("-fsanitize=thread");
                        else if (name == "undefined")
                            build_profile.cflags.push_back("-fsanitize=undefined");
                        else if (name == "memory")
                            build_profile.cflags.push_back("-fsanitize=memory");
                        else if (compiler.getType() == Compiler::Type::Clang && name == "leak")
                            build_profile.cflags.push_back("-fsanitize=leak");
                        break;

                    case Compiler::Type::MSVC:
                        if (name == "address")
                            build_profile.cflags.push_back("/fsanitize=address");
                        // No leak, memory, or thread sanitizers in MSVC
                        break;
                    }
                }
            }

            muuk::normalize_flags_inplace(build_profile.cflags, compiler);
            muuk::normalize_flags_inplace(build_profile.aflags, compiler);
            muuk::normalize_flags_inplace(build_profile.lflags, compiler);
            muuk::normalize_flags_inplace(build_profile.defines, compiler);

            muuk::logger::trace("Profile '{}' CFlags: {}", profile, fmt::join(build_profile.cflags, " "));
            muuk::logger::trace("Profile '{}' AFlags: {}", profile, fmt::join(build_profile.cflags, " "));
            muuk::logger::trace("Profile '{}' LFlags: {}", profile, fmt::join(build_profile.lflags, " "));
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

            const std::string src_path = util::file_system::to_unix_path(
                std::filesystem::absolute(
                    std::filesystem::path(raw_path))
                    .string());

            const std::string obj_path = util::file_system::sanitize_path(
                util::file_system::to_unix_path(
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

        /// Parses compilation targets from the `[library]` and `[build]` sections of the cache file.
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

            // TODO: Parse modules and sources from the compiler and platform sections

            if (has_modules)
                resolve_modules(build_manager, build_dir.string());
        };

        /// Parse libraries from the `[library]` section of the cache file. Generates archive targets.
        void parse_libraries(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const toml::value& muuk_file, const std::string& profile) {
            for (const auto& library_table : muuk_file.at("library").as_array()) {
                const auto library_name = library_table.at("name").as_string();
                const auto library_version = library_table.at("version").as_string();
                const auto lib_path_dir = (build_dir / library_table.at("path").as_string()).lexically_normal();

                const auto lib_path = util::file_system::to_unix_path(
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

                // Parse archive flags
                auto aflags = muuk::parse_array_as_vec(library_table.as_table(), "aflags");
                muuk::normalize_flags_inplace(aflags, compiler);
                build_manager.add_archive_target(lib_path, obj_files, aflags);

                muuk::logger::info("Added library target: {}", lib_path);
                muuk::logger::trace("  - Object Files: {}", fmt::join(obj_files, ", "));
                muuk::logger::trace("  - Archive Flags: {}", fmt::join(aflags, ", "));
            }
        };

        void parse_external_targets(BuildManager& build_manager, const toml::value& muuk_file, const std::string& profile, const fs::path& build_dir) {
            std::vector<ExternalTarget> externals;

            if (!muuk_file.contains("external"))
                return;

            for (const auto& entry : muuk_file.at("external").as_array()) {
                if (!entry.is_table())
                    continue;

                const auto& ext_table = entry.as_table();

                const std::string name = ext_table.at("name").as_string();
                const std::string version = ext_table.at("version").as_string();
                const std::string type = ext_table.at("type").as_string();
                const auto outputs = ext_table.at("outputs").as_array();
                const std::string base_path = ext_table.at("path").as_string();

                const auto source_path = util::file_system::to_unix_path(ext_table.at("path").as_string(), "../../");
                const std::string source_file = util::file_system::to_unix_path(ext_table.at("source").as_string(), "../../");
                const std::string cache_file = util::file_system::to_unix_path(base_path, "../../" + build_dir.string() + "/") + "/CMakeCache.txt";

                const std::string build_path = util::file_system::to_unix_path(
                    (build_dir / base_path).string(), "../../");

                std::vector<std::string> paths;
                for (const auto& out : outputs) {
                    const auto& out_table = out.as_table();
                    if (!out_table.contains("path"))
                        continue;

                    const auto output_path = util::file_system::to_unix_path(
                        (out_table.at("path").as_string()), "../../" + build_dir.string() + "/");

                    const auto profile_name = out_table.at("profile").as_string();
                    if (profile_name != profile)
                        continue;

                    paths.push_back(output_path);
                }

                build_manager.add_external_target(
                    type,
                    paths,
                    build_path,
                    source_path,
                    source_file,
                    cache_file);
            }
        }

        /// Parses builds from the TOML array
        void parse_executables(BuildManager& build_manager, const muuk::Compiler compiler, const std::filesystem::path& build_dir, const std::filesystem::path& build_artifact_dir, const std::string& profile_, const toml::value& muuk_file) {
            if (!muuk_file.contains("build") || !muuk_file.contains("library"))
                return;

            const auto& build_sections = muuk_file.at("build").as_array();
            const auto& library_sections = muuk_file.at("library").as_array();
            const auto& external_sections = muuk_file.at("external").as_array();

            // Index libraries by name + version
            std::unordered_map<std::string, std::unordered_map<std::string, toml::value>> lib_map;
            for (const auto& lib : library_sections) {
                const auto& lib_table = lib.as_table();
                const auto& name = lib_table.at("name").as_string();
                const auto& version = lib_table.at("version").as_string();
                lib_map[name][version] = lib;
            }

            // std::unordered_map<std::string, std::unordered_map<std::string, toml::value>> ext_map;
            // for (const auto& ext : external_sections) {
            //     const auto& ext_table = ext.as_table();
            //     const auto& name = ext_table.at("name").as_string();
            //     const auto& version = ext_table.at("version").as_string();
            //     ext_map[name][version] = ext;
            // }

            bool build_profile_match = false;

            for (const auto& entry : build_sections) {
                const auto& build_table = entry.as_table();
                const std::string executable_name = build_table.at("name").as_string();

                const auto link_type = build_link_from_string(build_table.at("link").as_string());

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

                std::string extension;
                switch (link_type) {
                case BuildLinkType::SHARED:
                    extension = SHARED_LIB_EXT; // e.g. ".dll", ".so", ".dylib"
                    break;
                case BuildLinkType::STATIC:
                    extension = LIB_EXT; // e.g. ".lib", ".a"
                    break;
                case BuildLinkType::EXECUTABLE:
                default:
                    extension = EXE_EXT; // e.g. ".exe" or ""
                    break;
                }

                const auto output_path = util::file_system::to_unix_path(
                    (build_dir / (executable_name + extension)).string(), "../../");

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

                            // If it doesn't have any sources or modules, then its an empty library so we should skip it
                            if (lib_table.contains("sources") || lib_table.contains("modules")) {
                                const auto lib_path = util::file_system::to_unix_path(
                                    (lib_path_dir / (lib_name + LIB_EXT)).string(), "../../");
                                libs.push_back(lib_path);
                            }
                        }

                        // TODO: Put some kind of warning so we don't add libs twice

                        // External dependencies (ie: CMake)
                        // if (ext_map.contains(lib_name) && ext_map.at(lib_name).contains(version)) {
                        //     const auto& ext_table = ext_map.at(lib_name).at(version).as_table();

                        //     // We need to get one of the outputs
                        //     const auto ext_path_dir = (build_artifact_dir / ext_table.at("path").as_string()).lexically_normal();

                        //     // If it doesn't have any sources or modules, then its an empty library so we should skip it
                        //     if (ext_table.contains("sources") || ext_table.contains("modules")) {
                        //         const auto lib_path = util::file_system::to_unix_path(
                        //             (ext_path_dir / (lib_name + LIB_EXT)).string(), "../../");
                        //         libs.push_back(lib_path);
                        //     }
                        // }
                    }
                }

                if (build_table.contains("libs")) {
                    for (const auto& lib : build_table.at("libs").as_array()) {
                        if (!lib.is_string())
                            continue;
                        libs.push_back(lib.as_string());
                    }
                }

                // Parse link flags
                std::vector<std::string> lflags = muuk::parse_array_as_vec(build_table, "lflags");
                muuk::normalize_flags_inplace(lflags, compiler);

                // Register build
                build_manager.add_link_target(
                    output_path,
                    obj_files,
                    libs,
                    lflags,
                    link_type);

                muuk::logger::info("Added link target: {}", output_path);
                muuk::logger::trace("  - Object Files: '{}'", fmt::join(obj_files, "', '"));
                muuk::logger::trace("  - Libraries: '{}'", fmt::join(libs, "', '"));
                muuk::logger::trace("  - Linker Flags: '{}'", fmt::join(lflags, "', '"));
            }

            if (!build_profile_match)
                muuk::logger::warn(
                    "No builds are enabled for the profile '{}'",
                    profile_);
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
            parse_external_targets(
                build_manager,
                muuk_file,
                profile,
                build_artifact_dir);
            parse_executables(
                build_manager,
                compiler,
                build_dir,
                build_artifact_dir,
                profile,
                muuk_file);

            return {};
        }

        std::tuple<std::string, std::string, std::string> get_profile_flag_strings(const BuildManager& manager, const std::string& profile) {
            const auto* build_profile = manager.get_profile(profile);
            std::string profile_cflags, profile_lflags, profile_aflags;

            if (build_profile) {
                std::ostringstream cflags_stream, lflags_stream, aflags_stream;

                for (const auto& flag : build_profile->cflags)
                    cflags_stream << flag << " ";
                for (const auto& def : build_profile->defines)
                    cflags_stream << def << " ";
                for (const auto& flag : build_profile->aflags)
                    aflags_stream << flag << " ";
                for (const auto& flag : build_profile->lflags)
                    lflags_stream << flag << " ";

                profile_cflags = cflags_stream.str();
                profile_aflags = aflags_stream.str();
                profile_lflags = lflags_stream.str();
            } else {
                muuk::logger::warn("No profile flags found for '{}'", profile);
            }

            return { profile_cflags, profile_aflags, profile_lflags };
        }
    } // namespace build
} // namespace muuk