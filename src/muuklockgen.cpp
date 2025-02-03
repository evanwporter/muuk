#include "../include/muuklockgen.h"
#include "../include/logger.h"

Package::Package(const std::string& name,
    const std::string& version,
    const std::string& base_path,
    const std::string& package_type)
    : name(name), version(version), base_path(base_path), package_type(package_type) {
    logger_ = Logger::get_logger("muuklockgen_package_logger");
}

void Package::merge(const Package& child_pkg) {
    logger_->info("[MuukLockGenerator::merger] Merging {} into {}", child_pkg.name, name);

    for (const auto& path : child_pkg.include) {
        include.insert((fs::path(child_pkg.base_path) / path).lexically_normal().string());
    }
    libflags.insert(child_pkg.libflags.begin(), child_pkg.libflags.end());
    lflags.insert(child_pkg.lflags.begin(), child_pkg.lflags.end());
    dependencies.insert(child_pkg.dependencies.begin(), child_pkg.dependencies.end());
}

toml::table Package::serialize() const {
    toml::table data;
    toml::array include_array, libflags_array, lflags_array, sources_array;

    for (const auto& path : include) include_array.push_back((fs::path(base_path) / path).lexically_normal().string());
    for (const auto& flag : libflags) libflags_array.push_back(flag);
    for (const auto& flag : lflags) lflags_array.push_back(flag);
    for (const auto& source : sources) sources_array.push_back((fs::path(base_path) / source).lexically_normal().string());

    data.insert("include", include_array);
    data.insert("libflags", libflags_array);
    data.insert("lflags", lflags_array);
    data.insert("sources", sources_array);
    data.insert("base_path", base_path);

    return data;
}

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) : base_path_(base_path) {
    logger_ = Logger::get_logger("muuklockgen_logger");
    logger_->info("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages_["library"] = {};
    resolved_packages_["build"] = {};
}

void MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
    logger_->info("Attempting to parse muuk.toml: {}", path);
    if (!fs::exists(path)) {
        logger_->error("Error: '{}' not found!", path);
        return;
    }

    MuukFiler muuk_filer(path);

    auto data = muuk_filer.get_config();

    logger_->info("Loaded TOML file successfully");

    if (!data.contains("package") || !data["package"].is_table()) {
        logger_->error("Missing 'package' section in TOML file.");
        return;
    }

    std::string package_name = *data["package"]["name"].value<std::string>();
    std::string package_version = *data["package"]["version"].value<std::string>();

    logger_->info("Parsing package: {} (version: {})", package_name, package_version);

    auto package = std::make_shared<Package>(std::string(package_name), std::string(package_version),
        fs::path(path).parent_path().string(), "library");
    if (data.contains("library")) {
        const auto& library_node = data["library"];
        if (const toml::table* library_table = library_node.as_table()) {
            if (library_table->contains(package_name)) {
                logger_->info("Found 'library' entry for package: {}", package_name);
                parse_section(*library_table->at(package_name).as_table(), *package);
            }
        }
        else {
            logger_->warn("No 'library' section found in TOML for {}", package_name);
        }
    }

    resolved_packages_["library"][package_name] = package;

    if (is_base && data.contains("build") && data["build"].is_table()) {
        for (const auto& [build_name, build_info] : *data["build"].as_table()) {
            logger_->info("Found build target: {}", build_name.str());

            auto build_package = std::make_shared<Package>(
                std::string(build_name.str()), std::string(package_version),
                fs::path(path).parent_path().string(), "build");
            parse_section(*build_info.as_table(), *build_package);
            resolved_packages_["build"][std::string(build_name.str())] = build_package;
        }
    }
}

void MuukLockGenerator::parse_section(const toml::table& section, Package& package) {
    logger_->info("Parsing section for package: {}", package.name);

    if (section.contains("include")) {
        for (const auto& inc : *section["include"].as_array()) {
            package.include.insert(*inc.value<std::string>());
            logger_->info("  Added include path: {}", *inc.value<std::string>());
        }
    }

    if (section.contains("sources")) {
        for (const auto& src : *section["sources"].as_array()) {
            package.sources.push_back(*src.value<std::string>());
            logger_->info("  Added source file: {}", *src.value<std::string>());
        }
    }

    if (section.contains("libflags")) {
        for (const auto& flag : *section["libflags"].as_array()) {
            package.libflags.insert(*flag.value<std::string>());
            logger_->info("  Added libflag: {}", *flag.value<std::string>());
        }
    }

    if (section.contains("lflags")) {
        for (const auto& flag : *section["lflags"].as_array()) {
            package.lflags.insert(*flag.value<std::string>());
            logger_->info("  Added lflag: {}", *flag.value<std::string>());
        }
    }

    if (section.contains("dependencies")) {
        for (const auto& [dep_name, dep_version] : *section["dependencies"].as_table()) {
            package.dependencies[std::string(dep_name.str())] = *dep_version.value<std::string>();
            logger_->info("  Added dependency: {} ({})", dep_name.str(), *dep_version.value<std::string>());
        }
    }
}

void MuukLockGenerator::resolve_dependencies(const std::string& package_name) {
    logger_->info("Resolving dependencies for: {}", package_name);

    auto package = resolved_packages_["library"].count(package_name)
        ? resolved_packages_["library"][package_name]
        : resolved_packages_["build"].count(package_name)
            ? resolved_packages_["build"][package_name]
            : nullptr;

            if (!package) {
                logger_->warn("Package '{}' not found, attempting to search for dependencies", package_name);
                search_and_parse_dependency(package_name);
                package = resolved_packages_["library"][package_name];
                if (!package) {
                    logger_->error("Error: Package '{}' not found after search.", package_name);
                    return;
                }
            }

            for (const auto& [dep_name, dep_version] : package->dependencies) {
                logger_->info("Resolving dependency '{}' for '{}'", dep_name, package_name);
                resolve_dependencies(dep_name);
                if (resolved_packages_["library"].count(dep_name)) {
                    logger_->info("Merging '{}' into '{}'", dep_name, package_name);
                    package->merge(*resolved_packages_["library"][dep_name]);
                }
            }
}

void MuukLockGenerator::search_and_parse_dependency(const std::string& package_name) {
    fs::path modules_dir = fs::path("modules"); // Use relative path

    if (!fs::exists(modules_dir)) return;

    for (const auto& dir_entry : fs::directory_iterator(modules_dir)) {
        if (dir_entry.is_directory() && dir_entry.path().filename().string().find(package_name) != std::string::npos) {
            fs::path dep_path = dir_entry.path() / "muuk.toml";
            if (fs::exists(dep_path)) {
                parse_muuk_toml(dep_path.string());
                return;
            }
        }
    }
}

void MuukLockGenerator::generate_lockfile(const std::string& output_path) {
    logger_->info("Generating muuk.lock.toml...");

    std::ofstream lockfile(output_path);
    if (!lockfile) {
        logger_->error("Failed to open lockfile: {}", output_path);
        return;
    }

    toml::table lock_data;
    for (const auto& [pkg_type, packages] : resolved_packages_) {
        logger_->info("Processing package type: {}", pkg_type);
        toml::table pkg_table;
        for (const auto& [pkg_name, pkg] : packages) {
            logger_->info("  Serializing package: {}", pkg_name);
            pkg_table.insert(pkg_name, pkg->serialize());
        }
        lock_data.insert(pkg_type, pkg_table);
    }

    logger_->info("Writing lockfile to: {}", output_path);
    lockfile << lock_data;
    lockfile.close();

    logger_->info("muuk.lock.toml generation complete!");
}
