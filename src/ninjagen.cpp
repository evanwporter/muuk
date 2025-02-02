#include "../include/ninjagen.h"
#include "../include/logger.h"
#include <fstream>

#ifdef _WIN32
constexpr bool is_windows = true;
constexpr const char* compiler = "cl";
constexpr const char* archiver = "lib";
constexpr const char* output_ext = ".obj";
#else
constexpr bool is_windows = false;
constexpr const char* compiler = "g++";
constexpr const char* archiver = "ar";
constexpr const char* output_ext = ".o";
#endif

// Constructor: Initializes the logger
NinjaGenerator::NinjaGenerator() {
    logger_ = Logger::get_logger("muukbuilder_logger");
}

void NinjaGenerator::GenerateNinjaFile(const std::string& lockfile_path) const {
    logger_->info("Starting Ninja build file generation...");
    logger_->info("Loading TOML file: {}", lockfile_path);

    if (!std::filesystem::exists(lockfile_path)) {
        logger_->error("Error: Lockfile '{}' not found!", lockfile_path);
        return;
    }

    // Load TOML data
    toml::table data;
    try {
        data = toml::parse_file(lockfile_path);
    }
    catch (const std::exception& e) {
        logger_->error("Error parsing TOML file: {}", e.what());
        return;
    }

    std::string ninja_file = "build.ninja";
    std::ofstream out(ninja_file);
    if (!out) {
        logger_->error("Error: Cannot open {} for writing.", ninja_file);
        return;
    }

    logger_->info("Generating Ninja build rules...");

    // Common Ninja build rules
    out << "# Auto-generated Ninja build file\n\n"
        << "cxx = " << compiler << "\n"
        << "ar = " << archiver << "\n\n"
        << "rule compile\n"
        << "  command = $cxx /c $in /Fo$out $cflags\n"
        << "  description = Compiling $in\n\n"
        << "rule archive\n"
        << "  command = $ar /OUT:$out $in\n"
        << "  description = Archiving $out\n\n"
        << "rule link\n"
        << "  command = $cxx $in /Fe:$out $lflags\n"
        << "  description = Linking $out\n\n";

    std::map<std::string, std::vector<std::string>> objects;
    std::vector<std::string> libraries;

    // Process packages
    for (const auto& [pkg_name, pkg_info] : data) {
        if (!pkg_info.is_table()) continue;

        auto& info = *pkg_info.as_table();
        logger_->info("Processing package: {}", pkg_name.str());

        std::vector<std::string> sources = get_list(info, "sources");
        std::vector<std::string> includes = get_list(info, "include");
        std::vector<std::string> lib_args = get_list(info, "libflags");
        std::vector<std::string> lflags = get_list(info, "lflags");

        std::vector<std::string> obj_files;
        std::string module_dir = "build/debug/" + std::string(pkg_name.str()) + "/";

        // Ensure the build directory exists
        std::filesystem::create_directories(module_dir);

        std::string lib_name = module_dir + std::string(pkg_name.str()) + ".lib";

        std::string cflags_common;
        for (const auto& inc : includes) {
            cflags_common += "/I" + normalize_path(inc) + " ";
        }
        for (const auto& flag : lib_args) {
            cflags_common += flag + " ";
        }
        for (const auto& flag : lflags) {
            cflags_common += flag + " ";
        }

        logger_->info("  Includes: {}", cflags_common);

        for (const auto& src : sources) {
            logger_->info("  Checking source file: {}", src);

            if (!std::filesystem::exists(src)) {
                logger_->warn("  Warning: Source file '{}' not found!", src);
            }

            std::string obj_file = module_dir + std::filesystem::path(src).stem().string() + output_ext;
            logger_->info("  Compiling: {} -> {}", src, obj_file);

            obj_files.push_back(obj_file);

            out << "build " << obj_file << ": compile " << src << "\n";
            out << "  cflags = " << cflags_common << "\n";

            logger_->info("  Wrote to build.ninja: {} -> {}", src, obj_file);
        }

        if (pkg_name != "muuk" && !obj_files.empty()) {
            out << "build " << lib_name << ": archive ";
            for (const auto& obj : obj_files) {
                out << obj << " ";
            }
            out << "\n";
            libraries.push_back(lib_name);

            logger_->info("  Creating static library: {}", lib_name);
        }
        else {
            objects[std::string(pkg_name.str())] = obj_files;
        }
    }

    // Collect linker flags and libraries
    std::string muuk_link_args;
    std::string muuk_objs;
    std::string muuk_libs;
    std::string dep_link_args;

    if (data.contains("muuk")) {
        auto& muuk_info = *data["muuk"].as_table();
        muuk_link_args = join(get_list(muuk_info, "lflags"), " ");
        muuk_objs = join(objects["muuk"], " ");
    }

    for (const auto& lib : libraries) {
        muuk_libs += lib + " ";
    }

    for (const auto& [pkg, info] : data) {
        if (pkg != "muuk") {
            dep_link_args += join(get_list(*info.as_table(), "lflags"), " ") + " ";
        }
    }

    std::string exe_output = "build/debug/muuk.exe";

    out << "\n# Link the final executable\n";
    out << "build " << exe_output << ": link " << muuk_objs << " " << muuk_libs << "\n";
    out << "  lflags = " << muuk_link_args << " " << dep_link_args << "\n";

    out.close();
    logger_->info("Generated {} successfully.", ninja_file);
}


// Helper functions
std::string NinjaGenerator::normalize_path(const std::string& path) {
    return std::filesystem::path(path).generic_string();
}

std::vector<std::string> NinjaGenerator::get_list(const toml::table& tbl, const std::string& key) {
    std::vector<std::string> result;
    if (tbl.contains(key)) {
        auto& arr = *tbl[key].as_array();
        for (auto& item : arr) {
            if (item.is_string()) {
                result.push_back(item.as_string()->get());
            }
        }
    }
    return result;
}

std::string NinjaGenerator::join(const std::vector<std::string>& vec, const std::string& sep) {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) result += sep;
        result += vec[i];
    }
    return result;
}
