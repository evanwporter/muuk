#include <string>

#include <glob/glob.h>
#include <toml++/toml.hpp>

#include "logger.h"
#include "muuklockgen.h"
#include "util.h"

template <typename Container>
static void print_array(std::ostringstream& stream, std::string_view key, const Container& values, bool new_line_at_end = true) {
    stream << key << " = [";
    bool first = true;
    for (const auto& value : values) {
        if (!first) {
            stream << ", ";
        }
        stream << "\"" << value << "\"";
        first = false;
    }
    if (new_line_at_end) {
        stream << "]\n";
    } else {
        stream << "]";
    }
}

Package::Package(const std::string& name, const std::string& version, const std::string& base_path, const std::string& package_type) :
    name(name),
    version(version),
    base_path(base_path),
    package_type(package_type) {
}

void Package::add_include_path(const std::string& path) {
    include.insert((fs::path(base_path) / path).lexically_normal().string());
}

void Package::merge(const Package& child_pkg) {

    muuk::logger::info("[MuukLockGenerator] Merging {} into {}", child_pkg.name, name);

    for (const auto& path : child_pkg.include) {
        include.insert(path);
    }
    cflags.insert(child_pkg.cflags.begin(), child_pkg.cflags.end());
    defines.insert(child_pkg.defines.begin(), child_pkg.defines.end());

    system_include.insert(child_pkg.system_include.begin(), child_pkg.system_include.end());

    deps.insert(child_pkg.deps.begin(), child_pkg.deps.end());

    for (const auto& [compiler, flags] : child_pkg.compiler_) {
        for (const auto& [flag_type, flag_values] : flags) {
            compiler_[compiler][flag_type].insert(flag_values.begin(), flag_values.end());
        }
    }

    for (const auto& [platform, flags] : child_pkg.platform_) {
        for (const auto& [flag_type, flag_values] : flags) {
            platform_[platform][flag_type].insert(flag_values.begin(), flag_values.end());
        }
    }

    // dependencies.insert(child_pkg.dependencies.begin(), child_pkg.dependencies.end());
}

std::string Package::serialize() const {
    std::ostringstream toml_stream; // string steam to allow more control over the final product
    toml::table data;
    toml::array
        include_array,
        cflags_array,
        sources_array,
        modules_array,
        libs_array,
        deps_array,
        profiles_array,
        defines_array;

    if (!include.empty()) {
        toml_stream << "include = [";
        for (const auto& path : include) {
            toml_stream << "  \"" << util::to_linux_path(path) << "\",";
        }
        toml_stream << "]\n";
    }

    print_array(toml_stream, "cflags", cflags);
    print_array(toml_stream, "defines", defines);

    if (!sources.empty()) {
        toml_stream << "sources = [\n";
        for (const auto& [source, source_cflags] : sources) {
            fs::path source_path = fs::path(base_path) / source;

            if (source.find('*') != std::string::npos) { // Handle globbing
                try {
                    muuk::logger::info("Globbing {}...", source_path.string());
                    std::vector<std::filesystem::path> globbed_paths = glob::glob(source_path.string());

                    for (const auto& path : globbed_paths) {
                        toml_stream << "  { path = \"" << util::to_linux_path(path.string()) << "\", cflags = [";
                        for (size_t i = 0; i < source_cflags.size(); ++i) {
                            toml_stream << "\"" << source_cflags[i] << "\"";
                            if (i != source_cflags.size() - 1)
                                toml_stream << ", ";
                        }
                        toml_stream << "] },\n";
                    }
                } catch (const std::exception& e) {
                    muuk::logger::warn("Error while globbing '{}': {}", source_path.string(), e.what());
                }
            } else { // Standard case without globbing
                toml_stream << "  { path = \"" << util::to_linux_path(source_path.lexically_normal().string()) << "\", cflags = [";
                for (size_t i = 0; i < source_cflags.size(); ++i) {
                    toml_stream << "\"" << source_cflags[i] << "\"";
                    if (i != source_cflags.size() - 1)
                        toml_stream << ", ";
                }
                toml_stream << "] },\n";
            }
        }
        toml_stream << "]\n";
    }

    toml_stream << "version = \"" << version << "\"\n";
    print_array(toml_stream, "libs", libs);
    print_array(toml_stream, "modules", modules);
    print_array(toml_stream, "dependencies", deps);
    print_array(toml_stream, "profiles", inherited_profiles);

    if (!platform_.empty()) {
        for (const auto& [platform, flags] : platform_) {
            toml_stream << "platform." << platform << " = { ";
            auto cflags_it = flags.find("cflags");
            if (cflags_it != flags.end()) {
                print_array(toml_stream, "cflags", cflags_it->second, false);
            }
            toml_stream << " }\n";
        }
    }

    if (!compiler_.empty()) {
        for (const auto& [compiler, flags] : compiler_) {
            toml_stream << "compiler." << compiler << " = { ";
            auto cflags_it = flags.find("cflags");
            if (cflags_it != flags.end()) {
                print_array(toml_stream, "cflags", cflags_it->second, false);
            }
            toml_stream << " }\n";
        }
    }

    return toml_stream.str();
}

void Package::enable_features(const std::unordered_set<std::string>& feature_set) {
    for (const std::string& feature : feature_set) {
        if (features.find(feature) != features.end()) {
            const auto& feature_data = features.at(feature);

            // Apply feature defines
            defines.insert(feature_data.defines.begin(), feature_data.defines.end());

            // Add feature dependencies
            for (const auto& dep : feature_data.dependencies) {
                deps.insert(dep);
            }

            muuk::logger::info("Enabled feature '{}' for package '{}'", feature, name);
        } else {
            muuk::logger::warn("Feature '{}' not found in package '{}'", feature, name);
        }
    }
}