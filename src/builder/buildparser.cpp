#include "../include/buildparser.hpp"
#include "../include/buildmanager.h"

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>
#include <toml++/toml.hpp>
#include "buildparser.hpp"

namespace fs = std::filesystem;

BuildParser::BuildParser(
    std::shared_ptr<BuildManager> manager,
    std::shared_ptr<MuukFiler> muuk_filer,
    muuk::compiler::Compiler compiler,
    fs::path build_dir,
    const std::string profile_
) :
    build_manager(std::move(manager)),
    compiler(compiler),
    muuk_filer(std::move(muuk_filer)),
    build_dir(build_dir),
    profile_(profile_)
{
}

void BuildParser::parse() {
    parse_compilation_targets();
    parse_libraries();
    parse_executables();
}

void BuildParser::parse_compilation_targets() {
    const auto& config_sections = muuk_filer->get_sections();

    for (const auto& [pkg_name, package_table] : config_sections) {
        if (!pkg_name.starts_with("build") && !pkg_name.starts_with("library")) continue;

        std::string package_name = pkg_name.substr(pkg_name.find('.') + 1);
        fs::path module_dir = build_dir / package_name;
        std::filesystem::create_directories(module_dir);

        std::vector<std::string> cflags = extract_flags(package_table, "cflags");
        std::vector<std::string> iflags = extract_flags(package_table, "include", "-I../../");

        // Extract platform and compiler-specific flags
        std::vector<std::string> platform_cflags = extract_platform_flags(package_table);
        std::vector<std::string> compiler_cflags = extract_compiler_flags(package_table);

        if (package_table.contains("sources")) {
            auto sources = package_table.at("sources").as_array();
            if (sources) {
                for (const auto& src_entry : *sources) {
                    if (!src_entry.is_table()) continue;
                    auto source_table = src_entry.as_table();
                    if (!source_table->contains("path")) continue;

                    std::string src_path = util::to_linux_path(
                        std::filesystem::absolute(
                            std::filesystem::path(
                                *source_table->at("path").value<std::string>()
                            )
                        ).string(),
                        "../../"
                    );

                    std::string obj_path = util::to_linux_path(
                        (module_dir / std::filesystem::path(src_path).stem()).string()
                        + OBJ_EXT,
                        "../../"
                    );

                    std::vector<std::string> src_cflags = extract_flags(*source_table, "cflags");
                    src_cflags.insert(src_cflags.end(), cflags.begin(), cflags.end());

                    // Add platform and compiler-specific flags
                    src_cflags.insert(src_cflags.end(), platform_cflags.begin(), platform_cflags.end());
                    src_cflags.insert(src_cflags.end(), compiler_cflags.begin(), compiler_cflags.end());

                    build_manager->add_compilation_target(src_path, obj_path, src_cflags, iflags);
                }
            }
        }
    }
}

void BuildParser::parse_libraries() {
    const auto& config_sections = muuk_filer->get_sections();

    for (const auto& [package_name, package_table] : config_sections) {
        if (!package_name.starts_with("library.")) continue;

        std::string lib_name = package_name.substr(8);
        std::string lib_path = util::to_linux_path(
            (build_dir / lib_name / (lib_name + LIB_EXT)).string(),
            "../../"
        );

        std::vector<std::string> obj_files;
        if (package_table.contains("sources")) {
            auto sources = package_table.at("sources").as_array();
            if (sources) {
                for (const auto& src_entry : *sources) {
                    if (!src_entry.is_table()) continue;
                    auto source_table = src_entry.as_table();
                    if (!source_table->contains("path")) continue;

                    std::string obj_file = util::to_linux_path(
                        (build_dir / lib_name / std::filesystem::path(*source_table->at("path").value<std::string>()).stem()).string() + OBJ_EXT,
                        "../../"
                    );
                    obj_files.push_back(obj_file);
                }
            }
        }

        std::vector<std::string> aflags = extract_flags(package_table, "aflags");
        build_manager->add_archive_target(lib_path, obj_files, aflags);
    }
}

void BuildParser::parse_executables() {
    const auto& config_sections = muuk_filer->get_sections();

    for (const auto& [build_name, build_table] : config_sections) {
        if (!build_name.starts_with("build.")) continue;

        std::string executable_name = build_name.substr(6); // Remove "build." prefix

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
                if (!profile_found) continue;
            }
        }

        std::string exe_path = util::to_linux_path(
            (build_dir / executable_name / (executable_name + EXE_EXT)).string(),
            "../../"
        );
        std::vector<std::string> obj_files;
        std::vector<std::string> libs;
        std::vector<std::string> iflags; // Include flags for header-only libraries

        muuk::logger::info(std::format("Parsing executable '{}'", executable_name));

        // Collect all source files -> Convert to object files
        if (build_table.contains("sources")) {
            auto sources = build_table.at("sources").as_array();
            if (sources) {
                for (const auto& src_entry : *sources) {
                    if (!src_entry.is_table()) continue;

                    auto source_table = src_entry.as_table();
                    if (!source_table->contains("path")) continue;

                    std::string src_path = util::to_linux_path(
                        std::filesystem::absolute(
                            std::filesystem::path(*source_table->at("path").value<std::string>())
                        ).string(),
                        "../../"
                    );

                    std::string obj_path = util::to_linux_path(
                        (build_dir / executable_name / std::filesystem::path(src_path).stem()).string()
                        + OBJ_EXT,
                        "../../"
                    );

                    obj_files.push_back(obj_path);
                }
            }
        }

        // Collect all dependencies (libraries & header-only includes)
        if (build_table.contains("dependencies")) {
            auto dependencies = build_table.at("dependencies").as_array();
            if (dependencies) {
                for (const auto& dep : *dependencies) {
                    std::string lib_name = *dep.value<std::string>();

                    // Check if the library has sources (meaning it's a compiled library)
                    if (config_sections.contains("library." + lib_name)) {
                        const auto& lib_table = config_sections.at("library." + lib_name);

                        if (lib_table.contains("sources")) {
                            // Library has sources, so we expect a .lib file
                            std::string lib_path = util::to_linux_path(
                                (build_dir / lib_name / (lib_name + LIB_EXT)).string(),
                                "../../"
                            );
                            libs.push_back(lib_path);
                        }

                        if (lib_table.contains("include")) {
                            // Header-only library, add include directories
                            std::vector<std::string> include_flags = extract_flags(lib_table, "include", "-I../../");
                            iflags.insert(iflags.end(), include_flags.begin(), include_flags.end());
                        }
                    }
                }
            }
        }

        // Collect linker flags
        std::vector<std::string> lflags = extract_flags(build_table, "lflags");

        // Add to build manager
        build_manager->add_link_target(exe_path, obj_files, libs, lflags);

        // Logging for debugging
        muuk::logger::trace(std::format("Added link target: {}", exe_path));

        muuk::logger::trace(std::format("  - Objects: {}",
            obj_files.empty() ? "None" : std::accumulate(std::next(obj_files.begin()), obj_files.end(), obj_files[0],
                [](std::string a, std::string b) { return std::move(a) + ", " + b; })));

        muuk::logger::trace(std::format("  - Libraries: {}",
            libs.empty() ? "None" : std::accumulate(std::next(libs.begin()), libs.end(), libs[0],
                [](std::string a, std::string b) { return std::move(a) + ", " + b; })));

        muuk::logger::trace(std::format("  - Include Flags: {}",
            iflags.empty() ? "None" : std::accumulate(std::next(iflags.begin()), iflags.end(), iflags[0],
                [](std::string a, std::string b) { return std::move(a) + ", " + b; })));

        muuk::logger::trace(std::format("  - Linker Flags: {}",
            lflags.empty() ? "None" : std::accumulate(std::next(lflags.begin()), lflags.end(), lflags[0],
                [](std::string a, std::string b) { return std::move(a) + ", " + b; })));
    }
}

std::vector<std::string> BuildParser::extract_flags(
    const toml::table& table,
    const std::string& key,
    const std::string& prefix
) {
    std::vector<std::string> flags;
    if (table.contains(key)) {
        auto flag_array = table.at(key).as_array();
        if (flag_array) {
            for (const auto& flag : *flag_array) {
                flags.push_back(prefix + *flag.value<std::string>());
            }
        }
    }
    return flags;
}

/** Extract platform-specific CFLAGS */
std::vector<std::string> BuildParser::extract_platform_flags(const toml::table& package_table) {
    std::vector<std::string> flags;
    if (!package_table.contains("platform")) return flags;

    auto platform_table = package_table.at("platform").as_table();
    if (!platform_table) return flags;

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
        if (!platform_entry) return flags;

        if (platform_entry->contains("cflags")) {
            auto cflags_array = platform_entry->at("cflags").as_array();
            if (cflags_array) {
                for (const auto& flag : *cflags_array) {
                    flags.push_back(*flag.value<std::string>());
                }
            }
        }
    }
    else {
        muuk::logger::warn("No configuration found for platform '{}'.", detected_platform);
    }

    return flags;
}

/** Extract compiler-specific CFLAGS */
std::vector<std::string> BuildParser::extract_compiler_flags(const toml::table& package_table) {
    std::vector<std::string> flags;
    if (!package_table.contains("compiler")) return flags;

    auto compiler_table = package_table.at("compiler").as_table();
    if (!compiler_table) return flags;

    if (compiler_table->contains(muuk::compiler::to_string(compiler))) {
        auto compiler_entry = compiler_table->at(muuk::compiler::to_string(compiler)).as_table();
        if (!compiler_entry) return flags;

        if (compiler_entry->contains("cflags")) {
            auto cflags_array = compiler_entry->at("cflags").as_array();
            if (cflags_array) {
                for (const auto& flag : *cflags_array) {
                    flags.push_back(*flag.value<std::string>());
                }
            }
        }
    }
    else {
        muuk::logger::warn("No configuration found for compiler '{}'.", muuk::compiler::to_string(compiler));
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
    if (profile_entry->contains("cflags")) {
        auto cflags_array = profile_entry->at("cflags").as_array();
        if (cflags_array) {
            for (const auto& flag : *cflags_array) {
                if (flag.is_string()) {
                    profile_cflags_str += muuk::normalize_flag(flag.value<std::string>().value_or(""), compiler) + " ";
                }
            }
            muuk::logger::info("Profile '{}' CFLAGS: {}", profile, profile_cflags_str);
        }
        else {
            muuk::logger::warn("Profile '{}' contains an invalid 'cflags' array.", profile);
        }
    }
    else {
        muuk::logger::warn("No 'cflags' found for profile '{}'.", profile);
    }

    // Handle LFLAGS extraction safely
    if (profile_entry->contains("lflags")) {
        auto lflags_array = profile_entry->at("lflags").as_array();
        if (lflags_array) {
            for (const auto& flag : *lflags_array) {
                if (flag.is_string()) {
                    profile_lflags_str += muuk::normalize_flag(flag.value<std::string>().value_or(""), compiler) + " ";
                }
            }
            muuk::logger::info("Profile '{}' LFLAGS: {}", profile, profile_lflags_str);
        }
        else {
            muuk::logger::warn("Profile '{}' contains an invalid 'lflags' array.", profile);
        }
    }
    else {
        muuk::logger::warn("No 'lflags' found for profile '{}'.", profile);
    }

    muuk::logger::info("Extracted profile_cflags: '{}'", profile_cflags_str);
    muuk::logger::info("Extracted profile_lflags: '{}'", profile_lflags_str);

    return { profile_cflags_str, profile_lflags_str };
}
