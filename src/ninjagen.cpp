#include "../include/muukfiler.h"
#include "../include/logger.h"
#include "../include/ninjagen.h"
#include "../include/util.h"
#include "../include/buildconfig.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <map>

namespace fs = std::filesystem;

NinjaGenerator::NinjaGenerator(const std::string& lockfile_path, const std::string& build_type)
    : lockfile_path_(lockfile_path), build_type_(build_type),
    compiler_("cl"), archiver_("lib"), ninja_file_("build.ninja"),
    build_dir_(fs::path("build") / build_type) {

    logger_ = Logger::get_logger("ninjagen_logger");  // Initialize logger
    logger_->info("[NinjaGenerator] Initializing NinjaGenerator with lockfile: '{}' and build type: '{}'", lockfile_path_, build_type_);
}

void NinjaGenerator::generate_ninja_file() {
    muuk_filer_ = std::make_unique<MuukFiler>(lockfile_path_);
    config_ = muuk_filer_->get_config();

    if (config_.empty()) {
        logger_->error("[NinjaGenerator:generate_ninja_file] Error: No TOML data loaded.");
        return;
    }

    fs::create_directories(build_dir_);
    logger_->info("[NinjaGenerator:generate_ninja_file] Created build directory: {}", build_dir_.string());

    std::ofstream out(ninja_file_);
    if (!out) {
        logger_->error("[NinjaGenerator:generate_ninja_file] Failed to create Ninja build file '{}'", ninja_file_);
        throw std::runtime_error("Failed to create Ninja build file.");
    }

    logger_->info("[NinjaGenerator:generate_ninja_file] Generating Ninja build rules...");
    write_ninja_header(out);

    auto [objects, libraries] = compile_objects(out);
    archive_libraries(out, objects, libraries);

    if (config_.contains("build")) {
        auto build_section = config_["build"].as_table();
        for (const auto& [build_name, _] : *build_section) {
            link_executable(out, objects, libraries, std::string(build_name.str()));
        }
    }

    logger_->info("[NinjaGenerator:generate_ninja_file] Ninja build file '{}' generated successfully!", ninja_file_);
}

void NinjaGenerator::write_ninja_header(std::ofstream& out) {
    logger_->info("[NinjaGenerator:write_ninja_header] Writing Ninja header...");

    out << "# Auto-generated Ninja build file\n\n"
        << "cxx = " << COMPILER << "\n"
        << "ar = " << ARCHIVER << "\n"
        << "linker = " << LINKER << "\n\n";

#ifdef _WIN32
    out << "rule compile\n"
        << "  command = $cxx /c $in /Fo$out $cflags\n"
        << "  description = Compiling $in\n\n"

        << "rule archive\n"
        << "  command = $ar /OUT:$out $in\n"
        << "  description = Archiving $out\n\n"

        << "rule link\n"
        << "  command = $linker $in /Fe$out $lflags $libraries\n"
        << "  description = Linking $out\n\n";
#else
    out << "rule compile\n"
        << "  command = $cxx -c $in -o $out $cflags\n"
        << "  description = Compiling $in\n\n"

        << "rule archive\n"
        << "  command = $ar $out $in\n"
        << "  description = Archiving $out\n\n"

        << "rule link\n"
        << "  command = $linker $in -o $out $lflags $libraries\n"
        << "  description = Linking $out\n\n";
#endif
}

std::pair<std::map<std::string, std::vector<std::string>>, std::vector<std::string>>
NinjaGenerator::compile_objects(std::ofstream& out) {
    std::map<std::string, std::vector<std::string>> objects;
    std::vector<std::string> libraries;

    for (const auto& [pkg_type, packages] : config_) {
        if (pkg_type != "library" && pkg_type != "build") continue;

        auto packages_table = packages.as_table();
        if (!packages_table) continue;

        for (const auto& [pkg_name, pkg_info] : *packages_table) {
            logger_->info("[NinjaGenerator:compile_objects] Processing package: {}", pkg_name.str());
            fs::path module_dir = build_dir_ / std::string(pkg_name.str());
            fs::create_directories(module_dir);

            std::vector<std::string> obj_files;
            auto pkg_table = pkg_info.as_table();
            std::string cflags_common;

            if (pkg_table->contains("include")) {
                auto includes = pkg_table->at("include").as_array();
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

            if (pkg_table->contains("cflags")) {
                auto cflags_array = pkg_table->at("cflags").as_array();
                if (cflags_array) {
                    std::vector<std::string> flag_list;
                    for (const auto& flag : *cflags_array) {
                        flag_list.push_back(*flag.value<std::string>());
                    }
                    cflags_common += util::normalize_flags(flag_list);
                }
            }

            if (pkg_table && pkg_table->contains("sources")) {
                auto sources = pkg_table->at("sources").as_array();
                if (sources) {
                    for (const auto& src : *sources) {
                        std::string src_path = util::to_linux_path(*src.value<std::string>());
                        std::string obj_path = util::to_linux_path((module_dir / fs::path(src_path).stem()).string() + ".obj");
                        obj_files.push_back(obj_path);

                        logger_->info("[NinjaGenerator:compile_objects] Compiling: {} -> {}", src_path, obj_path);
                        out << "build " << obj_path << ": compile " << src_path << "\n";
                        out << "  cflags =" << cflags_common << "\n";
                    }
                }
            }

            objects[std::string(pkg_name)] = obj_files;
        }
    }

    return { objects, libraries };
}

void NinjaGenerator::archive_libraries(std::ofstream& out,
    const std::map<std::string, std::vector<std::string>>& objects,
    std::vector<std::string>& libraries) {

    for (const auto& [pkg_name, obj_files] : objects) {
        if (obj_files.empty()) continue;

        fs::path lib_name = build_dir_ / pkg_name / (pkg_name + ".lib");
        std::string normalized_lib = util::to_linux_path(lib_name.string());

        logger_->info("[NinjaGenerator:archive_libraries] Creating library: {}", normalized_lib);
        out << "build " << normalized_lib << ": archive";
        for (const auto& obj : obj_files) {
            out << " " << util::to_linux_path(obj);
        }
        out << "\n";

        libraries.push_back(normalized_lib);
    }
}

void NinjaGenerator::link_executable(std::ofstream& out,
    const std::map<std::string, std::vector<std::string>>& objects,
    const std::vector<std::string>& libraries,
    const std::string& build_name) {

    fs::path exe_output = build_dir_ / build_name / (build_name + EXE_EXT);
    std::string normalized_exe = util::to_linux_path(exe_output.string());

    logger_->info("[NinjaGenerator:link_executable] Linking executable for '{}': {}", build_name, normalized_exe);

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

    out << "\nbuild " << normalized_exe << ": link " << build_objs << lib_files << "\n"
        << "  lflags =" << lflags << "\n"
        << "  libraries =" << lib_files << "\n";
}