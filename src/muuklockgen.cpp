#include "../include/muuklockgen.h"
#include "../include/logger.h"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// **Constructor: Initializes the logger**
MuukLockGenerator::MuukLockGenerator(const std::string& modules_path, const std::string& lockfile_path, const std::string& root_toml)
    : modules_dir(modules_path), lockfile(lockfile_path), root_toml_path(root_toml) {

    logger_ = Logger::get_logger("muukbuilder_logger");

}

void MuukLockGenerator::generateLockFile() {
    logger_->info("Starting lockfile generation...");

    loadPackages();
    resolveDependencies();
    writeLockFile();

    logger_->info("Lockfile generation complete.");
}

void MuukLockGenerator::loadPackages() {
    logger_->info("Loading packages from TOML files...");

    auto parsePackage = [&](const std::string& path, bool isRoot = false) {
        logger_->info("Parsing package file: {}", path);

        if (!fs::exists(path)) {
            logger_->error("TOML file '{}' does not exist!", path);
            return;
        }

        toml::table data;
        try {
            data = toml::parse_file(path);
        }
        catch (const std::exception& e) {
            logger_->error("Error parsing TOML file '{}': {}", path, e.what());
            return;
        }

        std::string name = data["package"]["name"].value_or("");
        if (isRoot) base_package = name;
        logger_->info("Processing package: {}", name);

        fs::path module_dir = fs::path(path).parent_path();
        auto resolvePath = [&](const std::string& val) {
            std::string resolved = (module_dir / val).lexically_normal().string();
            std::replace(resolved.begin(), resolved.end(), '\\', '/'); // Convert all backslashes to forward slashes
            return resolved;
            };

        auto getArray = [&](const char* key, bool resolve) {
            std::vector<std::string> vec;
            if (auto arr = data["build"][key].as_array()) {
                for (const auto& val : *arr) {
                    std::string entry = val.value_or("");
                    vec.push_back(resolve ? resolvePath(entry) : entry);
                }
            }
            return vec;
            };

        Package pkg{
            getArray("include", true),
            getArray("sources", true),
            getArray("libflags", false),
            getArray("lflags", false)
        };

        if (auto deps = data["dependencies"].as_table()) {
            for (const auto& [key, _] : *deps) {
                pkg.dependencies.push_back(std::string(key.str()));
            }
        }

        logger_->info("  Includes: {}", pkg.include.size());
        logger_->info("  Sources: {}", pkg.sources.size());
        logger_->info("  Libflags: {}", pkg.libflags.size());
        logger_->info("  Lflags: {}", pkg.lflags.size());
        logger_->info("  Dependencies: {}", pkg.dependencies.size());

        logger_->info("  Found {} includes, {} sources, {} dependencies",
            pkg.include.size(), pkg.sources.size(), pkg.dependencies.size());

        packages[name] = std::move(pkg);
        };

    if (fs::exists(root_toml_path)) {
        parsePackage(root_toml_path, true);
    }

    for (const auto& entry : fs::directory_iterator(modules_dir)) {
        if (fs::is_directory(entry) && fs::exists(entry.path() / "muuk.toml")) {
            parsePackage((entry.path() / "muuk.toml").string());
        }
    }

    logger_->info("Package loading complete.");
}

void MuukLockGenerator::resolveDependencies() {
    logger_->info("Resolving package dependencies...");

    std::unordered_map<std::string, int> dependency_count;
    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (const auto& [name, pkg] : packages) {
        dependency_count[name] = pkg.dependencies.size();
        for (const auto& dep : pkg.dependencies) graph[dep].push_back(name);
    }

    std::set<std::string> resolved;
    if (!base_package.empty()) {
        build_order.push_back(base_package);
        resolved.insert(base_package);
    }

    while (resolved.size() < packages.size()) {
        for (auto it = dependency_count.begin(); it != dependency_count.end();) {
            if (resolved.count(it->first) || it->second > 0) {
                ++it;
                continue;
            }
            build_order.push_back(it->first);
            resolved.insert(it->first);
            for (const auto& dependent : graph[it->first]) dependency_count[dependent]--;
            it = dependency_count.erase(it);
        }
    }

    logger_->info("Resolved {} packages.", build_order.size());
}

void MuukLockGenerator::writeLockFile() {
    logger_->info("Writing lockfile: {}", lockfile);

    std::ofstream out(lockfile);
    if (!out) {
        logger_->error("Error: Could not open '{}' for writing!", lockfile);
        return;
    }

    std::unordered_map<std::string, std::set<std::string>> resolved_includes;
    std::set<std::string> accumulated_lflags;
    std::set<std::string> base_package_includes;

    for (const auto& pkg_name : build_order) {
        auto& pkg = packages[pkg_name];
        std::set<std::string> includes(pkg.include.begin(), pkg.include.end());

        for (const auto& dep : pkg.dependencies) {
            if (resolved_includes.find(dep) != resolved_includes.end()) {
                includes.insert(resolved_includes[dep].begin(), resolved_includes[dep].end());
            }
        }

        resolved_includes[pkg_name] = includes;
        accumulated_lflags.insert(pkg.lflags.begin(), pkg.lflags.end());

        if (pkg_name != base_package) {
            base_package_includes.insert(includes.begin(), includes.end());
        }
    }

    if (!base_package.empty()) {
        resolved_includes[base_package].insert(base_package_includes.begin(), base_package_includes.end());
    }

    bool first = true;
    for (const auto& pkg_name : build_order) {
        if (!first) out << "\n";
        first = false;

        auto& pkg = packages[pkg_name];
        out << "[" << pkg_name << "]\n";

        auto writeArray = [&](const char* key, const std::vector<std::string>& values) {
            if (!values.empty()) { // Ensure empty arrays are not skipped
                out << key << " = [";
                for (size_t i = 0; i < values.size(); ++i) {
                    out << "\"" << values[i] << "\"" << (i + 1 < values.size() ? ", " : "");
                }
                out << "]\n";
            }
            };

        std::vector<std::string> includes(resolved_includes[pkg_name].begin(), resolved_includes[pkg_name].end());
        writeArray("include", includes);
        writeArray("sources", pkg.sources);
        writeArray("libflags", pkg.libflags);
        writeArray("lflags", pkg.lflags);
    }

    logger_->info("Lockfile '{}' successfully written.", lockfile);
}
