#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "base_config.hpp"

// TODO: Put inside namespace muuk::lockgen
// namespace muuk {
//     namespace lockgen {

typedef std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>>
    Profiles;

PackageType::PackageType(Type type) :
    type_(type) { }

bool PackageType::operator==(const PackageType& other) const {
    return type_ == other.type_;
}

Result<void> Dependency::load(const std::string name_, const toml::value& v) {
    name = name_;
    if (v.is_string()) {
        version = v.as_string();
        return {};
    }
    if (!v.is_table()) {
        return Err("Invalid dependency format for '{}'", name_);
    }

    git_url = toml::find_or<std::string>(v, "git", "");
    path = toml::find_or<std::string>(v, "path", "");
    version = toml::find_or<std::string>(v, "version", "");

    auto enabled_feats = toml::find_or<std::vector<std::string>>(v, "features", {});
    enabled_features = std::unordered_set<std::string>(enabled_feats.begin(), enabled_feats.end());

    libs = toml::find_or<std::vector<std::string>>(v, "libs", {});
    return {};
}

Result<void> Dependency::serialize(toml::value& out) const {
    if (name.empty())
        return Err("Dependency name is empty");
    out["name"] = name; // If name is empty then we have a problem
    if (!git_url.empty())
        out["git"] = git_url;
    if (!path.empty())
        out["path"] = path;
    if (!version.empty())
        out["version"] = version;

    if (!enabled_features.empty()) {
        toml::array features;
        for (const auto& feat : enabled_features)
            features.push_back(feat);
        out["features"] = features;
    }

    if (!libs.empty()) {
        toml::array lib_array;
        for (const auto& lib : libs)
            lib_array.push_back(lib);
        out["libs"] = lib_array;
    }
    return {};
}

toml::value source_file::serialize() const {
    return toml::table {
        { "path", path },
        { "cflags", cflags }
    };
}

void CompilerConfig::load(const toml::value& v, const std::string& base_path) {
    BaseFields::load(v, base_path);
}

void PlatformConfig::load(const toml::value& v, const std::string& base_path) {
    BaseFields::load(v, base_path);
}

void Compilers::load(const toml::value& v, const std::string& base_path) {
    if (v.contains("clang"))
        clang.load(v.at("clang"), base_path);
    if (v.contains("gcc"))
        gcc.load(v.at("gcc"), base_path);
    if (v.contains("msvc"))
        msvc.load(v.at("msvc"), base_path);
}

void Compilers::merge(const Compilers& other) {
    clang.merge(other.clang);
    gcc.merge(other.gcc);
    msvc.merge(other.msvc);
}

void Compilers::serialize(toml::value& out) const {
    toml::value compiler;
    clang.serialize(compiler["clang"]);
    if (compiler["clang"].as_table().empty())
        compiler.as_table().erase("clang");

    gcc.serialize(compiler["gcc"]);
    if (compiler["gcc"].as_table().empty())
        compiler.as_table().erase("gcc");

    msvc.serialize(compiler["msvc"]);
    if (compiler["msvc"].as_table().empty())
        compiler.as_table().erase("msvc");

    if (!compiler.as_table().empty())
        out["compiler"] = compiler;
}

void Platforms::load(const toml::value& v, const std::string& base_path) {
    if (v.contains("windows"))
        windows.load(v.at("windows"), base_path);
    if (v.contains("linux"))
        linux.load(v.at("linux"), base_path);
    if (v.contains("apple"))
        apple.load(v.at("apple"), base_path);
}

void Platforms::merge(const Platforms& other) {
    windows.merge(other.windows);
    linux.merge(other.linux);
    apple.merge(other.apple);
}

void Platforms::serialize(toml::value& out) const {
    toml::value platform = toml::table {};

    windows.serialize(platform["windows"]);
    if (platform.at("windows").as_table().empty())
        platform.as_table().erase("windows");

    linux.serialize(platform["linux"]);
    if (platform.at("linux").as_table().empty())
        platform.as_table().erase("linux");

    apple.serialize(platform["apple"]);
    if (platform.at("apple").as_table().empty())
        platform.as_table().erase("apple");

    if (!platform.as_table().empty())
        out["platform"] = platform;
}

void ProfileConfig::load(const toml::value& v, const std::string& profile_name, const std::string& base_path) {
    BaseConfig<ProfileConfig>::load(v, base_path);
    name = profile_name;
    inherits = toml::find_or<std::vector<std::string>>(v, "inherits", {});
}

void Library::External::load(const toml::value& v) {
    type = toml::find_or<std::string>(v, "type", "");
    path = toml::find_or<std::string>(v, "path", "");
    args = toml::find_or<std::vector<std::string>>(v, "args", {});
    outputs = toml::find_or<std::vector<std::string>>(v, "outputs", {});
}

void Library::External::serialize(toml::value& out) const {
    // BaseConfig<Library>::serialize(out);

    toml::value external_section = toml::table {};
    if (!type.empty())
        external_section["type"] = type;
    if (!path.empty())
        external_section["path"] = path;
    if (!args.empty())
        external_section["args"] = args;
    if (!outputs.empty())
        external_section["outputs"] = outputs;

    out["external"] = external_section;
}

void Library::load(const std::string& name_, const std::string& version_, const std::string& base_path_, const toml::value& v) {
    name = name_;
    version = version_;

    BaseConfig<Library>::load(v, base_path_);
    if (v.contains("external"))
        external.load(toml::find(v, "external"));
}

void Library::serialize(toml::value& out) const {
    out["name"] = name;
    out["version"] = version;
    BaseConfig<Library>::serialize(out);
    external.serialize(out);
}

void Build::merge(const Package& package) {
    // Merge base fields
    include.insert(package.library_config.include.begin(), package.library_config.include.end());
    cflags.insert(package.library_config.cflags.begin(), package.library_config.cflags.end());
    cxxflags.insert(package.library_config.cxxflags.begin(), package.library_config.cxxflags.end());
    aflags.insert(package.library_config.aflags.begin(), package.library_config.aflags.end());
    lflags.insert(package.library_config.lflags.begin(), package.library_config.lflags.end());
    defines.insert(package.library_config.defines.begin(), package.library_config.defines.end());
    libs.insert(package.library_config.libs.begin(), package.library_config.libs.end());

    // Merge shared_ptr dependencies directly
    all_dependencies_array.insert(
        package.all_dependencies_array.begin(),
        package.all_dependencies_array.end());
}

Result<void> Build::serialize(toml::value& out) const {
    BaseConfig<Build>::serialize(out);

    // Collect dependencies and sort them by (name, version)
    std::vector<std::shared_ptr<Dependency>> sorted_deps;
    for (const auto& dep_ptr : all_dependencies_array) {
        if (dep_ptr) {
            sorted_deps.push_back(dep_ptr);
        }
    }

    std::sort(sorted_deps.begin(), sorted_deps.end(), [](const auto& a, const auto& b) {
        if (a->name == b->name)
            return a->version < b->version;
        return a->name < b->name;
    });

    toml::array dep_array;
    for (const auto& dep_ptr : sorted_deps) {
        toml::value dep_entry;
        auto dep_ser = dep_ptr->serialize(dep_entry);
        if (!dep_ser)
            return Err(dep_ser);
        dep_array.push_back(dep_entry);
    }

    out["profiles"] = profiles;

    out["dependencies"] = dep_array;
    out.at("dependencies").as_array_fmt().fmt = toml::array_format::multiline;
    return {};
}

void Build::load(const toml::value& v, const std::string& base_path) {
    BaseConfig<Build>::load(v, base_path);

    profiles = util::muuk_toml::find_or_get<std::unordered_set<std::string>>(v, "profile", {});

    for (const auto& [dep_name, versions] : dependencies)
        for (const auto& [dep_version, dep] : versions)
            all_dependencies_array.insert(std::make_shared<Dependency>(dep));
}

// } // namespace lockgen
// } // namespace muuk
