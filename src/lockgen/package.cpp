#include <string>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "lockgen/config/base.hpp"
#include "lockgen/config/package.hpp"
#include "logger.hpp"
#include "util.hpp"

const PackageType PackageType::LIBRARY(PackageType::Type::LIBRARY);
const PackageType PackageType::BUILD(PackageType::Type::BUILD);

Package::Package(const std::string& name, const std::string& version, const std::string& base_path, const PackageType package_type) :
    name(name),
    version(version),
    base_path(base_path),
    package_type(package_type) {
}

void Package::merge(const Package& child_pkg) {

    muuk::logger::info("[MuukLockGenerator] Merging {} into {}", child_pkg.name, name);

    util::array_ops::merge(
        all_dependencies_array,
        child_pkg.all_dependencies_array);

    library_config.merge(child_pkg.library_config);
    compilers_config.merge(child_pkg.compilers_config);
    platforms_config.merge(child_pkg.platforms_config);
    // dependencies.insert(child_pkg.dependencies.begin(), child_pkg.dependencies.end());
}

// TODO: Need to update
void Package::enable_features(const std::unordered_set<std::string>& feature_set) {
    for (const std::string& feature : feature_set) {
        if (features.find(feature) != features.end()) {
            const auto& feature_data = features.at(feature);

            // Apply feature defines
            util::array_ops::merge(library_config.defines, feature_data.defines);
            util::array_ops::merge(library_config.undefines, feature_data.undefines);

            // TODO Fix
            // Add feature dependencies
            // for (const auto& dep : feature_data.dependencies)
            //     deps.insert(dep);

            muuk::logger::info("Enabled feature '{}' for package '{}'", feature, name);
        } else {
            muuk::logger::warn("Feature '{}' not found in package '{}'", feature, name);
        }
    }
}

void Package::serialize_dependencies(std::ostringstream& toml_stream, const DependencyVersionMap<std::shared_ptr<Dependency>> dependencies_, const std::string section_name) {
    if (dependencies_.empty())
        return;

    toml_stream << section_name << " = [\n";

    for (const auto& [dep_name, versions] : dependencies_) {
        for (const auto& [dep_version, dep_ptr] : versions) {
            if (!dep_ptr) {
                muuk::logger::warn("Skipping dependency '{}' (version '{}') due to null pointer.", dep_name, dep_version);
                continue;
            }

            toml_stream << "  { name = \"" << dep_name << "\", version = \"" << dep_version << "\" },\n";
        }
    }

    toml_stream << "]\n";
}
