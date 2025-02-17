#include "../include/muukfiler.h"
#include "../include/logger.h"
#include "../include/ninjagen.h"
#include "../include/buildconfig.h"
#include "../include/util.h"


#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include "ninjagen.h"

namespace fs = std::filesystem;

NinjaGenerator::NinjaGenerator(const std::string& lockfile_path, const std::string& build_type,
    const std::string& compiler, const std::string& archiver, const std::string& linker)
    : lockfile_path_(lockfile_path), build_type_(build_type), compiler_(compiler),
    archiver_(archiver), linker_(linker), ninja_file_("build.ninja"),
    build_dir_(fs::path("build") / build_type) {

    logger_ = logger::get_logger("ninjagen_logger");
    logger_->info("Initializing with Compiler: '{}', Archiver: '{}', Linker: '{}'", compiler_, archiver_, linker_);
}

void NinjaGenerator::generate_ninja_file(const std::string& target_build) {
    // TODO: target build  
    (void)target_build;

    logger_->info(" Generating Ninja build script...");
    logger_->info("----------------------------------");

    muuk_filer_ = std::make_unique<MuukFiler>(lockfile_path_);
    config_ = muuk_filer_->get_config();

    std::vector<std::string> section_order = muuk_filer_->parse_section_order();

    logger_->info("Keys in config_ (ordered from TOML):");

    for (const auto& section : section_order) {
        logger_->info("  {}", section);
    }

    if (config_.empty()) {
        logger_->error("Error: No TOML data loaded.");
        return;
    }

    extract_platform_flags();

    fs::create_directories(build_dir_);
    logger_->info("Created build directory: {}", build_dir_.string());

    std::ofstream out(ninja_file_);
    if (!out) {
        logger_->error("Failed to create Ninja build file '{}'", ninja_file_);
        throw std::runtime_error("Failed to create Ninja build file.");
    }

    logger_->info("Generating Ninja build rules...");
    write_ninja_header(out);

    // Parse dependencies
    std::unordered_map<std::string, std::vector<std::string>> dependencies_map;
    std::unordered_map<std::string, std::vector<std::string>> modules_map;

    for (const auto& package_name : section_order) {
        if (!package_name.starts_with("library.")) continue;

        auto package_table = config_.at_path(package_name).as_table();
        if (!package_table) continue;

        std::string lib_name = package_name.substr(8);

        if (package_table->contains("dependencies")) {
            auto deps_array = package_table->at("dependencies").as_array();
            if (deps_array) {
                for (const auto& dep : *deps_array) {
                    dependencies_map[lib_name].push_back(dep.value<std::string>().value_or(""));
                }
            }
        }

        if (package_table->contains("modules")) {
            auto modules_array = package_table->at("modules").as_array();
            if (modules_array) {
                for (const auto& mod : *modules_array) {
                    modules_map[lib_name].push_back(mod.value<std::string>().value_or(""));
                }
            }
        }
    }

    auto [objects, libraries] = compile_objects(out, dependencies_map, modules_map);
    archive_libraries(out, objects, libraries);

    if (config_.contains("build")) {
        auto build_section = config_["build"].as_table();
        for (const auto& [build_name, _] : *build_section) {
            link_executable(out, objects, libraries, std::string(build_name.str()));
        }
    }

    logger_->info("Ninja build file '{}' generated successfully!", ninja_file_);
}

void NinjaGenerator::write_ninja_header(std::ofstream& out) {
    logger_->info("Writing Ninja header...");

    out << "# ------------------------------------------------------------\n"
        << "# Auto-generated Ninja build file\n"
        << "# Generated by Muuk\n"
        << "# ------------------------------------------------------------\n\n";

    out << "# Toolchain Configuration\n"
        << "cxx = " << compiler_ << "\n"
        << "ar = " << archiver_ << "\n"
        << "linker = " << linker_ << "\n\n";

    std::string module_dir = util::to_linux_path((build_dir_ / "modules/").string());

    util::ensure_directory_exists(module_dir);
    logger_->info("Created module build directory: {}", module_dir);

    std::string platform_cflags_str;
    for (const auto& cflag : platform_cflags_) {
        platform_cflags_str += cflag + " ";
    }

    out << "# Platform-Specific Flags\n"
        << "platform_cflags = " << platform_cflags_str << "\n\n";

    out << "# ------------------------------------------------------------\n"
        << "# Rules for Compiling C++ Modules\n"
        << "# ------------------------------------------------------------\n";

    if (compiler_ == "cl") {
        // MSVC Compiler
        out << "rule module_compile\n"
            << "  command = $cxx /std:c++20 /utf-8 /c /interface $in /Fo$out /IFC:"
            << module_dir << " /ifcOutput "
            << module_dir << " /ifcSearchDir "
            << module_dir << " $cflags $platform_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    }
    else if (compiler_ == "clang++") {
        // Clang Compiler
        out << "rule module_compile\n"
            << "  command = $cxx -std=c++20 -fmodules-ts -c $in -o $out -fmodule-output="
            << module_dir << " $cflags $platform_cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    }
    else {
        // GCC Compiler
        out << "rule module_compile\n"
            << "  command = $cxx -std=c++20 -fmodules-ts -c $in -o $out -fmodule-output="
            << module_dir << " $cflags\n"
            << "  description = Compiling C++ module $in\n\n";
    }

    out << "# ------------------------------------------------------------\n"
        << "# Compilation, Archiving, and Linking Rules\n"
        << "# ------------------------------------------------------------\n";

    if (compiler_ == "cl") {
        // MSVC (cl)
        out << "rule compile\n"
            << "  command = $cxx /c $in /ifcSearchDir "
            << module_dir
            << " /Fo$out $cflags\n"
            << "  description = Compiling $in\n\n"

            << "rule archive\n"
            << "  command = $ar /OUT:$out $in\n"
            << "  description = Archiving $out\n\n"

            << "rule link\n"
            << "  command = $linker $in /OUT:$out $lflags $libraries\n"
            << "  description = Linking $out\n\n";
    }
    else {
        // MinGW or Clang on Windows / Unix
        out << "rule compile\n"
            << "  command = $cxx -c $in -o $out $cflags\n"
            << "  description = Compiling $in\n\n"

            << "rule archive\n"
            << "  command = $ar rcs $out $in\n"
            << "  description = Archiving $out\n\n"

            << "rule link\n"
            << "  command = $linker $in -o $out $lflags $libraries\n"
            << "  description = Linking $out\n\n";
    }
}

std::pair<std::unordered_map<std::string, std::vector<std::string>>, std::vector<std::string>>
NinjaGenerator::compile_objects(std::ofstream& out,
    const std::unordered_map<std::string, std::vector<std::string>>& dependencies_map,
    const std::unordered_map<std::string, std::vector<std::string>>& modules_map) {

    std::unordered_map<std::string, std::vector<std::string>> objects;
    std::vector<std::string> libraries;

    std::vector<std::string> section_order = muuk_filer_->parse_section_order();

    std::string previous_module_obj;
    std::unordered_map<std::string, std::vector<std::string>> compiled_modules;

    for (const auto& package_name : section_order) {
        if (!package_name.starts_with("build") && !package_name.starts_with("library")) continue;

        size_t dot_pos = package_name.find('.');
        if (dot_pos == std::string::npos) {
            logger_->error("Invalid package format in 'muuk.lock.toml': {}", package_name);
            continue;
        }

        std::string pkg_type = package_name.substr(0, dot_pos);
        std::string pkg_name = package_name.substr(dot_pos + 1);

        if (pkg_type != "library" && pkg_type != "build") continue;

        auto package_table = config_.at_path(package_name).as_table();
        if (!package_table) continue;

        logger_->info("Processing package: {}", package_name);

        fs::path module_dir = build_dir_ / pkg_name;
        fs::create_directories(module_dir);

        std::vector<std::string> obj_files;
        std::string cflags_common;

        std::vector<std::string> dependency_module_paths;
        std::vector<std::string> dependency_module_objects;

        if (dependencies_map.contains(pkg_name)) {
            for (const auto& dep : dependencies_map.at(pkg_name)) {
                if (modules_map.contains(dep)) {
                    std::string module_path = "build/" + build_type_ + "/" + dep + "/modules";
                    cflags_common += " /IFC:" + module_path;
                    dependency_module_paths.push_back(module_path);

                    if (compiled_modules.contains(dep)) {
                        for (const auto& mod_obj : compiled_modules[dep]) {
                            dependency_module_objects.push_back(mod_obj);
                        }
                    }
                }
            }
        }

        if (package_table->contains("include")) {
            auto includes = package_table->at("include").as_array();
            if (includes) {
                for (const auto& inc : *includes) {
#ifdef _WIN32
                    cflags_common += " /I" + util::to_linux_path(*inc.value<std::string>());
#else
                    cflags_common += " -I" + util::to_linux_path(*inc.value<std::string>());
#endif
                }
            }
        }

        if (package_table->contains("cflags")) {
            auto cflags_array = package_table->at("cflags").as_array();
            if (cflags_array) {
                std::vector<std::string> flag_list;
                for (const auto& flag : *cflags_array) {
                    flag_list.push_back(*flag.value<std::string>());
                }
                cflags_common += util::normalize_flags(flag_list);
            }
        }

        if (package_table->contains("modules")) {
            auto modules = package_table->at("modules").as_array();
            if (modules) {
                for (const auto& mod : *modules) {
                    std::string mod_path = util::to_linux_path(fs::absolute(fs::path(*mod.value<std::string>())).string());

                    if (mod_path.ends_with(".ixx")) {
                        std::string module_name = fs::path(mod_path).stem().string();
                        std::string obj_path = util::to_linux_path((module_dir / module_name).string() + OBJ_EXT);

                        logger_->info("Compiling module '{}' -> {}", module_name, obj_path);

                        // Ensure each module depends on:
                        // 1. The previous module in the sequence
                        // 2. All modules from dependencies
                        out << "build " << obj_path << ": module_compile " << mod_path;
                        if (!previous_module_obj.empty() || !dependency_module_objects.empty()) {
                            out << " |";
                            if (!previous_module_obj.empty()) {
                                out << " " << previous_module_obj;
                            }
                            for (const auto& dep_obj : dependency_module_objects) {
                                out << " " << dep_obj;
                            }
                        }
                        out << "\n";
                        out << "  cflags = " << cflags_common << "\n";

                        // Update previous module object file
                        previous_module_obj = obj_path;
                        compiled_modules[pkg_name].push_back(obj_path);
                        obj_files.push_back(obj_path);
                    }
                }
            }
        }

        if (package_table->contains("sources")) {
            auto sources = package_table->at("sources").as_array();
            if (sources) {
                for (const auto& src : *sources) {
                    std::string src_path = util::to_linux_path(fs::absolute(fs::path(*src.value<std::string>())).string());
                    std::string obj_path = util::to_linux_path((module_dir / fs::path(src_path).stem()).string() + OBJ_EXT);
                    obj_files.push_back(obj_path);

                    logger_->info("Compiling source: {} -> {}", src_path, obj_path);

                    // Ensure that each source depends on the last compiled module
                    out << "build " << obj_path << ": compile " << src_path;
                    if (!previous_module_obj.empty()) {
                        out << " | " << previous_module_obj;
                    }
                    out << "\n  cflags = " << cflags_common << "\n";
                }
            }
        }

        objects[std::string(pkg_name)] = obj_files;
    }

    out << "\n";
    return { objects, libraries };
}



void NinjaGenerator::archive_libraries(std::ofstream& out,
    const std::unordered_map<std::string, std::vector<std::string>>& objects,
    std::vector<std::string>& libraries) {

    for (const auto& [pkg_name, obj_files] : objects) {
        if (!config_.contains("library") || !config_["library"].as_table() || !config_["library"].as_table()->contains(pkg_name)) {
            logger_->info("Skipping '{}' because it's not a library.", pkg_name);
            continue;
        }

        fs::path lib_name = build_dir_ / pkg_name / (pkg_name + LIB_EXT);
        std::string normalized_lib = util::to_linux_path(lib_name.string());

        std::vector<std::string> input_files = obj_files;

        if (config_["library"].as_table() && config_["library"][pkg_name].as_table()) {
            toml::v3::array* lib_entries = config_["library"][pkg_name]["libs"].as_array();
            if (lib_entries) {
                for (const toml::v3::node& lib : *lib_entries) {
                    input_files.push_back(util::to_linux_path(*lib.value<std::string>()));
                }
            }
        }

        if (input_files.empty()) {
            logger_->info("Skipping library '{}' because it has no sources or linked libraries.", pkg_name);
            continue;
        }

        logger_->info("Creating library: {}", normalized_lib);
        out << "build " << normalized_lib << ": archive";
        for (const auto& file : input_files) {
            out << " " << file;
        }
        out << "\n";

        libraries.push_back(normalized_lib);
    }
}

void NinjaGenerator::link_executable(std::ofstream& out,
    const std::unordered_map<std::string, std::vector<std::string>>& objects,
    const std::vector<std::string>& libraries,
    const std::string& build_name) {

    fs::path exe_output = build_dir_ / build_name / (build_name + EXE_EXT);
    std::string normalized_exe = util::to_linux_path(exe_output.string());

    logger_->info("Linking executable for '{}': {}", build_name, normalized_exe);

    std::string build_objs;
    if (objects.find(build_name) != objects.end()) {
        for (const auto& obj : objects.at(build_name)) {
            build_objs += util::to_linux_path(obj) + " ";
        }
    }

    std::string lib_files;
    for (const auto& lib : libraries) {
        lib_files += util::to_linux_path(lib) + " ";
    }

    std::string lflags;
    if (config_["build"].as_table() && config_["build"][build_name].as_table()) {
        auto lflags_array = config_["build"][build_name]["lflags"].as_array();
        if (lflags_array) {
            std::vector<std::string> flag_list;
            for (const auto& flag : *lflags_array) {
                flag_list.push_back(*flag.value<std::string>());
            }
            lflags += util::normalize_flags(flag_list);
        }
    }

    // TODO: Only add this when its in debug mode
#ifdef _WIN32
    lflags += " /DEBUG ";
#else
    lflags += " -g ";
#endif

    // // TODO: Find a better location for this
    // lflags += "/LTCG";

    out << "\nbuild " << normalized_exe << ": link " << build_objs << lib_files << "\n"
        << "  lflags =" << lflags << "\n"
        << "  libraries = " << lib_files << "\n";
}

void NinjaGenerator::extract_platform_flags() {
    logger_->info("Extracting platform-specific flags from lockfile...");

    // Check if the "platform" section exists
    if (!config_.contains("platform")) {
        logger_->warn("No 'platform' section found in lockfile.");
        return;
    }

    auto platform_table = config_["platform"].as_table();
    if (!platform_table) {
        logger_->warn("Platform section found but is not a valid table.");
        return;
    }

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

    logger_->info("Detected platform: {}", detected_platform);

    if (!platform_table->contains(detected_platform)) {
        logger_->warn("No configuration found for platform '{}'.", detected_platform);
        return;
    }

    auto flags_table = platform_table->at(detected_platform).as_table();
    if (!flags_table || !flags_table->contains("cflags")) {
        logger_->warn("No 'cflags' entry found for platform '{}'.", detected_platform);
        return;
    }

    auto flags_array = flags_table->at("cflags").as_array();
    if (!flags_array) {
        logger_->warn("'flags' entry for platform '{}' is not an array.", detected_platform);
        return;
    }

    // Extract flags
    platform_cflags_.clear();
    for (const auto& flag : *flags_array) {
        if (flag.is_string()) {
            std::string flag_value = flag.value<std::string>().value_or("");
            platform_cflags_.push_back(flag_value);
            logger_->info("Added platform-specific flag: {}", flag_value);
        }
        else {
            logger_->warn("Ignoring non-string flag entry for platform '{}'.", detected_platform);
        }
    }

    // Log the final list of extracted flags
    std::string platform_cflags_str;
    for (const auto& flag : platform_cflags_) {
        platform_cflags_str += flag + " ";
    }

    if (!platform_cflags_.empty()) {
        logger_->info("Final platform-specific flags: {}", platform_cflags_str);
    }
    else {
        logger_->warn("No valid platform-specific flags were extracted.");
    }
}

