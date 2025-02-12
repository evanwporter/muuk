#include "../include/muuklockgen.h"
#include "../include/logger.h"
#include "../include/util.h"
#include "../include/buildconfig.h"

#include <glob/glob.hpp>
#include "muuklockgen.h"

Package::Package(const std::string& name,
    const std::string& version,
    const std::string& base_path,
    const std::string& package_type)
    : name(name), version(version), base_path(base_path), package_type(package_type) {
    logger_ = Logger::get_logger("muuklockgen_package_logger");
}

void Package::merge(const Package& child_pkg) {
    if (&child_pkg == nullptr) {
        logger_->error("[MuukLockGenerator] 'child_pkg' is NULL or corrupted. Cannot merge into '{}'", name);
        return;
    }

    logger_->info("[MuukLockGenerator] Merging {} into {}", child_pkg.name, name);

    for (const auto& path : child_pkg.include) {
        include.insert((fs::path(child_pkg.base_path) / path).lexically_normal().string());
    }
    cflags.insert(child_pkg.cflags.begin(), child_pkg.cflags.end());

    system_include.insert(child_pkg.system_include.begin(), child_pkg.system_include.end());

    // dependencies.insert(child_pkg.dependencies.begin(), child_pkg.dependencies.end());
}

toml::table Package::serialize() const {
    toml::table data;
    toml::array include_array, cflags_array, sources_array, modules_array, libs_array;

    for (const auto& path : include) include_array.push_back((fs::path(base_path) / path).lexically_normal().string());

    for (const auto& flag : cflags) cflags_array.push_back(flag);

    for (const auto& source : sources) {
        fs::path source_path = fs::path(base_path) / source;

        if (source.find('*') != std::string::npos) {
            std::vector<std::string> matched_files;
            try {
                std::vector<std::filesystem::path> globbed_paths = glob::glob(source_path.string());

                for (const auto& path : globbed_paths) {
                    matched_files.push_back(path.string());
                }
            }
            catch (const std::exception& e) {
                logger_->warn("[muuk::clean] Error while globbing '{}': {}", source_path.string(), e.what());
            }

        }
        else {
            sources_array.push_back(source_path.lexically_normal().string());
        }
    }

    for (const auto& lib : libs) {
        libs_array.push_back(lib);
    }

    for (const auto& module : modules) modules_array.push_back(module);

    data.insert("include", include_array);
    data.insert("cflags", cflags_array);
    data.insert("sources", sources_array);
    data.insert("base_path", base_path);
    data.insert("modules", modules_array);
    data.insert("libs", libs_array);

    return data;
}

MuukLockGenerator::MuukLockGenerator(const std::string& base_path) : base_path_(base_path) {
    logger_ = Logger::get_logger("muuklockgen_logger");
    logger_->info("MuukLockGenerator initialized with base path: {}", base_path_);

    resolved_packages_["library"] = {};
    resolved_packages_["build"] = {};

    module_parser_ = std::make_unique<MuukModuleParser>(base_path);
}

void MuukLockGenerator::parse_muuk_toml(const std::string& path, bool is_base) {
    // logger_->info("Attempting to parse muuk.toml: {}", path);
    if (!fs::exists(path)) {
        logger_->error("Error: '{}' not found!", path);
        return;
    }

    MuukFiler muuk_filer(path);
    auto data = muuk_filer.get_config();

    if (!data.contains("package") || !data["package"].is_table()) {
        logger_->error("Missing 'package' section in TOML file.");
        return;
    }

    std::string package_name = data["package"]["name"].value_or<std::string>("unknown");
    std::string package_version = data["package"]["version"].value_or<std::string>("0.0.0");

    logger_->info("Parsing muuk.toml for package: {} ({})", package_name, package_version);

    logger_->info("Parsing package: {} (version: {}) with muuk path: `{}`", package_name, package_version, path);

    auto package = std::make_shared<Package>(std::string(package_name), std::string(package_version),
        fs::path(path).parent_path().string(), "library");

    if (data.contains("library")) {
        const auto& library_node = data["library"];
        if (const toml::table* library_table = library_node.as_table()) {
            if (library_table->contains(package_name)) {
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
    // logger_->info("Parsing section for package: {}", package.name);

    if (section.contains("include")) {
        for (const auto& inc : *section["include"].as_array()) {
            package.include.insert(*inc.value<std::string>());
            logger_->debug(" Added include path: {}", *inc.value<std::string>());
        }
    }

    if (section.contains("sources")) {
        for (const auto& src : *section["sources"].as_array()) {
            package.sources.push_back(*src.value<std::string>());
            logger_->debug(" Added source file: {}", *src.value<std::string>());
        }
    }

    if (section.contains("cflags")) {
        for (const auto& flag : *section["cflags"].as_array()) {
            package.cflags.insert(*flag.value<std::string>());
        }
    }

    if (section.contains("gflags")) {
        for (const auto& flag : *section["gflags"].as_array()) {
            package.gflags.insert(*flag.value<std::string>());
            package.cflags.insert(*flag.value<std::string>());

        }
    }

    if (section.contains("dependencies")) {
        logger_->debug("Dependencies of '{}':", package.name);
        for (const auto& [dep_name, dep_value] : *section["dependencies"].as_table()) {
            std::map<std::string, std::string> dependency_info;
            std::string muuk_path;
            std::string version = "unknown";

            if (dep_value.is_string()) {
                dependency_info["version"] = *dep_value.value<std::string>();
            }
            else if (dep_value.is_table()) {
                for (const auto& [key, val] : *dep_value.as_table()) {
                    if (val.is_string()) {
                        if (key.str() == "version") {
                            version = *val.value<std::string>();
                            dependency_info["version"] = version;
                        }
                        else if (key.str() == "muuk_path") {
                            muuk_path = *val.value<std::string>();

                            if (fs::exists(muuk_path)) {
                                dependency_info["muuk_path"] = muuk_path;
                            }
                            else {
                                logger_->warn("muuk_path '{}' does not exist for dependency '{}'. Ignoring.", muuk_path, std::string(dep_name.str()));
                            }
                        }
                    }
                }
            }
            else {
                logger_->error("Invalid dependency format for '{}'. Expected a string or a table.", std::string(dep_name.str()));
                continue;
            }

            package.dependencies[std::string(dep_name.str())] = dependency_info;

            if (!muuk_path.empty()) {
                logger_->debug("  → Dependency '{}' (version: {}, muuk_path: {})", std::string(dep_name.str()), version, muuk_path);
            }
            else {
                logger_->debug("  → Dependency '{}' (version: {})", std::string(dep_name.str()), version);
            }
        }
    }

    if (section.contains("libs")) {
        for (const auto& lib : *section["libs"].as_array()) {
            package.libs.push_back(*lib.value<std::string>());
            logger_->debug(" Added library: {}", *lib.value<std::string>());
        }
    }

    std::vector<std::string> module_paths;
    if (section.contains("modules")) {
        for (const auto& mod : *section["modules"].as_array()) {
            module_paths.push_back(*mod.value<std::string>());
            logger_->info("  Found module source: {}", *mod.value<std::string>());
        }
    }

    if (!module_paths.empty()) {
        process_modules(module_paths, package);
    }
}

void MuukLockGenerator::resolve_dependencies(const std::string& package_name, std::optional<std::string> search_path) {
    if (visited.count(package_name)) {
        logger_->warn("Circular dependency detected for '{}'. Skipping resolution.", package_name);
        return;
    }

    visited.insert(package_name);
    logger_->info("Resolving dependencies for: {} with muuk path: '{}'", package_name, search_path.value_or(""));

    auto package = resolved_packages_["library"].count(package_name)
        ? resolved_packages_["library"][package_name]
        : resolved_packages_["build"].count(package_name)
            ? resolved_packages_["build"][package_name]
            : nullptr;

            if (!package) {
                if (search_path) {
                    fs::path search_file = fs::path(search_path.value());
                    if (!search_path.value().ends_with("muuk.toml")) {
                        logger_->info("Search path '{}' does not end with 'muuk.toml', appending it.", search_file.string());
                        search_file /= "muuk.toml";
                    }
                    else {
                        logger_->info("Search path '{}' already ends with 'muuk.toml', using as is.", search_file.string());
                    }

                    if (fs::exists(search_file)) {
                        parse_muuk_toml(search_file.string());
                        package = resolved_packages_["library"][package_name];

                        if (!package) {
                            logger_->error("Error: Package '{}' not found after parsing '{}'.", package_name, *search_path);
                            return;
                        }
                    }
                }
                else {
                    // If no search path, search in dependency folders
                    search_and_parse_dependency(package_name);
                    package = resolved_packages_["library"][package_name];

                    if (!package) {
                        logger_->error("Error: Package '{}' not found after search.", package_name);
                        return;
                    }
                }
                // logger_->info("Package '{}' not found in resolved packages.", package_name);


            }

            for (const auto& [dep_name, dep_info] : package->dependencies) {
                logger_->info("Resolving dependency '{}' for '{}'", dep_name, package_name);

                std::string dep_search_path;
                if (dep_info.count("muuk_path")) {
                    dep_search_path = dep_info.at("muuk_path");
                    logger_->info("Using defined muuk_path for dependency '{}': {}", dep_name, dep_search_path);
                }

                if (dep_info.count("system")) {
                    resolve_system_dependency(dep_name, package);
                }
                else {
                    // Resolve dependency, passing the search path if available
                    resolve_dependencies(dep_name, dep_search_path.empty() ? std::nullopt : std::optional<std::string>{ dep_search_path });
                }
                if (resolved_packages_["library"].count(dep_name)) {
                    logger_->info("Merging '{}' into '{}'", dep_name, package_name);
                    if (package) {
                        package->merge(*resolved_packages_["library"][dep_name]);
                    }
                }
            }

            logger_->info("Added '{}' to resolved order list.", package_name);
            resolved_order_.push_back(package_name);
}

void MuukLockGenerator::search_and_parse_dependency(const std::string& package_name) {
    logger_->info("Searching for target package '{}'.", package_name);
    fs::path modules_dir = fs::path(DEPENDENCY_FOLDER);

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
    logger_->warn("Dependency '{}' not found in '{}'", package_name, modules_dir.string());
}

void MuukLockGenerator::generate_lockfile(const std::string& output_path, bool is_release) {
    logger_->info("Generating muuk.lock.toml...");

    std::ofstream lockfile(output_path);
    if (!lockfile) {
        logger_->error("Failed to open lockfile: {}", output_path);
        return;
    }

    std::set<std::string> gflags;

    logger_->info("Extracting global flags from build packages...");
    for (const auto& [pkg_name, pkg] : resolved_packages_["build"]) {
        logger_->info("Extracting gflags from build package: {}", pkg_name);

        std::string build_gflags_str;
        for (const auto& flag : pkg->gflags) {
            build_gflags_str += flag + " ";
            gflags.insert(flag);
        }

        if (!build_gflags_str.empty()) {
            logger_->info("  → Flags from '{}': {}", pkg_name, build_gflags_str);
        }
        else {
            logger_->info("  → No flags found in '{}'", pkg_name);
        }
    }

    if (!is_release) {
        logger_->info("Applying debug-specific flags...");
        gflags.insert("-g");
        gflags.insert("/Od");
        gflags.insert("-DDEBUG");
        gflags.insert("/W4");
        // gflags.insert("/MDdb");
        // gflags.insert("-fsanitize=address");
        // gflags.insert("-fsanitize=undefined");
        // gflags.insert("-fno-omit-frame-pointer");
#ifdef __linux__
        gflags.insert("-rdynamic");
#endif
    }
    else {
        logger_->info("Applying release-specific flags...");
#ifdef _MSC_VER
        gflags.insert("/O2");   // Full optimization
        gflags.insert("/DNDEBUG");
        gflags.insert("/GL");   // Whole program optimization
        gflags.insert("/Oi");   // Enable intrinsic functions
        gflags.insert("/Gy");   // Function-level linking
#else
        gflags.insert("-O3");   // Max optimization
        gflags.insert("-DNDEBUG");
        gflags.insert("-march=native");
        gflags.insert("-mtune=native");
#endif
    }

#ifdef DEBUG
    std::string gflags_str;
    for (const auto& flag : gflags) {
        gflags_str += flag + " ";
    }

    logger_->info("Applying global flags to all library packages: {}", gflags_str);
#endif


    for (auto& [pkg_name, pkg] : resolved_packages_["library"]) {
        if (!pkg) {
            logger_->error("Package '{}' is null! Skipping.", pkg_name);
            continue;
        }

        if (pkg->cflags.empty()) {
            logger_->info("Package '{}' has no cflags defined. Initializing cflags set before applying gflags.", pkg_name);
        }

        pkg->cflags.insert(gflags.begin(), gflags.end());

        logger_->info("Applied gflags to package: {}", pkg_name);
    }

    toml::table lock_data;
    for (const auto& package_name : resolved_order_) {
        auto package_opt = find_package(package_name);
        if (!package_opt) continue;

        auto package = package_opt.value();
        std::string package_type = package->package_type;

        toml::table package_table = package->serialize();

        lockfile << "[" << package_type << "." << package_name << "]\n";
        lockfile << package_table << "\n\n";

        logger_->info("Written package '{}' to lockfile.", package_name);
    }


    lockfile << lock_data;
    lockfile.close();

    logger_->info("muuk.lock.toml generation complete!");
}

std::optional<std::shared_ptr<Package>> MuukLockGenerator::find_package(const std::string& package_name) {
    if (resolved_packages_["library"].count(package_name)) {
        return resolved_packages_["library"][package_name];
    }

    if (resolved_packages_["build"].count(package_name)) {
        return resolved_packages_["build"][package_name];
    }
    return std::nullopt;
}

void MuukLockGenerator::process_modules(const std::vector<std::string>& module_paths, Package& package) {
    logger_->info("Processing modules for package: {}", package.name);

    module_parser_->parseAllModules(module_paths);

    std::vector<std::string> resolved_modules = module_parser_->resolveAllModules();

    for (const auto& mod : resolved_modules) {
        package.modules.push_back(mod);
        logger_->info("Resolved and added module '{}' to package '{}'", mod, package.name);
    }
}

void MuukLockGenerator::resolve_system_dependency(const std::string& package_name, std::optional<std::shared_ptr<Package>> package) {
    logger_->info("Checking system dependency: '{}'", package_name);

    std::string include_path, lib_path;

    // Special handling for Python
    if (package_name == "python" || package_name == "python3") {
#ifdef _WIN32
        include_path = util::execute_command("python -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::execute_command("python -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#else
        include_path = util::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_path('include'))\"");
        lib_path = util::execute_command("python3 -c \"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
#endif
    }
    // Special handling for Boost
    else if (package_name == "boost") {
#ifdef _WIN32
        include_path = util::execute_command("where boostdep");
        lib_path = util::execute_command("where boost");
#else
        include_path = util::execute_command("boostdep --include-path");
        lib_path = util::execute_command("boostdep --lib-path");
#endif
    }
    else {
#ifdef _WIN32
        logger_->warn("System dependency '{}' resolution on Windows is not fully automated. Consider setting paths manually.", package_name);
#else
        include_path = util::execute_command("pkg-config --cflags-only-I " + package_name + " | sed 's/-I//' | tr -d '\n'");
        lib_path = util::execute_command("pkg-config --libs-only-L " + package_name + " | sed 's/-L//' | tr -d '\n'");
#endif
    }

    if (!include_path.empty() && util::path_exists(include_path)) {
        system_include_paths_.insert(include_path);
        if (package) (*package)->include.insert(include_path);
        logger_->info("  - Found Include Path: {}", include_path);
    }
    else {
        logger_->warn("  - Include path for '{}' not found.", package_name);
    }

    if (!lib_path.empty() && fs::exists(lib_path)) {
        system_library_paths_.insert(lib_path);
        if (package) (*package)->libs.push_back(lib_path);
        logger_->info("  - Found Library Path: {}", lib_path);
    }
    else {
        logger_->warn("  - Library path for '{}' not found.", package_name);
    }

    if (include_path.empty() && lib_path.empty()) {
        logger_->error("Failed to resolve system dependency '{}'. Consider installing it or specifying paths manually.", package_name);
    }
}
