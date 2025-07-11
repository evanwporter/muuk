#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include <glob/glob.hpp>
#include <toml.hpp>

#include "compiler.hpp"
#include "lockgen/config/base.hpp"
#include "lockgen/config/build.hpp"
#include "lockgen/config/package.hpp"
#include "rustify.hpp"
#include "toml_ext.hpp"

namespace fs = std::filesystem;

#define LOAD_IF_PRESENT(key, target) \
    if (v.contains(#key))            \
    target.load(v.at(#key), base_path)

/// Forces a TOML table to be serialized in a single line.
inline void force_oneline(toml::value& v) {
    if (v.is_table())
        v.as_table_fmt().fmt = toml::table_format::oneline;
}

#define SERIALIZE_SUBCONFIG(KEY, FIELD, TARGET) \
    do {                                        \
        toml::value tmp = toml::table {};       \
        (FIELD).serialize(tmp);                 \
        if (!tmp.as_table().empty()) {          \
            force_oneline(tmp);                 \
            (TARGET)[KEY] = tmp;                \
        }                                       \
    } while (0) // do-while is used to avoid macro issues

namespace muuk {
    namespace lockgen {

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

            enabled_features = toml::try_find_or<std::unordered_set<std::string>>(v, "features", {});

            libs = toml::find_or<std::vector<std::string>>(v, "libs", {});
            return {};
        }

        Result<void> Dependency::serialize(toml::value& out) const {
            // If name is empty then we have a problem
            // Something got messed along the way and it probably
            // should've not gotten to this point lol
            if (name.empty())
                return Err("Dependency name is empty");

            out["name"] = name;

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

        void Compilers::load(const toml::value& v, const std::string& base_path) {
            LOAD_IF_PRESENT(clang, clang);
            LOAD_IF_PRESENT(gcc, gcc);
            LOAD_IF_PRESENT(msvc, msvc);
        }

        void Compilers::merge(const Compilers& other) {
            clang.merge(other.clang);
            gcc.merge(other.gcc);
            msvc.merge(other.msvc);
        }

        void Compilers::serialize(toml::value& out) const {
            toml::value compiler_out = toml::table {};

            SERIALIZE_SUBCONFIG("clang", clang, compiler_out);
            SERIALIZE_SUBCONFIG("gcc", gcc, compiler_out);
            SERIALIZE_SUBCONFIG("msvc", msvc, compiler_out);

            if (!compiler_out.as_table().empty()) {
                force_oneline(compiler_out);
                out["compiler"] = compiler_out;
            }
        }

        void Platforms::load(const toml::value& v, const std::string& base_path) {
            LOAD_IF_PRESENT(windows, windows);
            LOAD_IF_PRESENT(linux, linux);
            LOAD_IF_PRESENT(apple, apple);
        }

        void Platforms::merge(const Platforms& other) {
            windows.merge(other.windows);
            linux.merge(other.linux);
            apple.merge(other.apple);
        }

        void Platforms::serialize(toml::value& out) const {
            toml::value platform_out = toml::table {};

            SERIALIZE_SUBCONFIG("apple", apple, platform_out);
            SERIALIZE_SUBCONFIG("linux", linux, platform_out);
            SERIALIZE_SUBCONFIG("windows", windows, platform_out);

            if (!platform_out.as_table().empty()) {
                force_oneline(platform_out);
                out["platform"] = platform_out;
            }
        }

        void ProfileConfig::load(const toml::value& v, const std::string& profile_name, const std::string& base_path) {
            BaseConfig<ProfileConfig>::load(v, base_path);
            name = profile_name;
            inherits = toml::try_find_or<std::vector<std::string>>(
                v,
                "inherits",
                {});
            lockgen::load(settings, v);
            lockgen::load(sanitizers, v);
        }

        void ProfileConfig::serialize(toml::value& out) const {
            BaseConfig<ProfileConfig>::serialize(out);

            // Serialize inheritance
            if (!inherits.empty())
                out["inherits"] = inherits;

            lockgen::serialize(settings, out);
            lockgen::serialize(sanitizers, out);
        }

        void Library::load(const std::string& name_, const std::string& version_, const std::string& base_path_, const toml::value& v) {
            name = name_;
            version = version_;

            BaseConfig<Library>::load(v, base_path_);
        }

        void Library::serialize(toml::value& out, Platforms platforms_, Compilers compilers_) const {
            out["name"] = name;
            out["version"] = version;
            BaseConfig<Library>::serialize(out);
            out["profiles"] = profiles;

            platforms_.serialize(out);
            compilers_.serialize(out);
        }

        void External::load(const std::string& name_, const std::string& version_, const std::string& base_path_, const toml::value& v) {
            name = name_;
            version = version_;

            fs::path base_path = base_path_;
            path = base_path.string();

            // TODO: If type is empty then what are we doing here?
            type = toml::try_find_or<std::string>(v, "type", "");
            args = toml::try_find_or<std::vector<std::string>>(v, "args", {});

            if (type == "cmake")
                source_file = (base_path / "CMakeLists.txt").string();

            if (v.contains("outputs")) {
                // Same thing here
                for (const auto& entry : v.at("outputs").as_array()) {
                    if (!entry.is_table())
                        continue;
                    ExternalOutput out;

                    // TODO Can be string or table

                    out.path = (base_path / toml::try_find_or(entry, "path", std::string {})).lexically_normal().string();
                    out.profile = toml::try_find_or(entry, "profile", std::string {});

                    outputs.push_back(std::move(out));
                }
            }
        }

        void External::serialize(toml::value& out) const {
            if (name.empty())
                return;

            out["name"] = name;
            out["version"] = version;
            out["source"] = source_file;
            out["path"] = path;

            if (!type.empty())
                out["type"] = type;

            if (!args.empty())
                out["args"] = args;

            if (!outputs.empty()) {
                toml::array outputs_array;
                for (const auto& output : outputs) {
                    toml::value entry = toml::table {};

                    entry["path"] = output.path;
                    entry["profile"] = output.profile;

                    outputs_array.push_back(entry);
                }

                out["outputs"] = outputs_array;
            }
        }

        void Build::merge(const Package& package) {
            // Merge base fields
            using util::array_ops::merge;
            merge(include, package.library_config.include);
            merge(cflags, package.library_config.cflags);
            merge(cxxflags, package.library_config.cxxflags);
            merge(aflags, package.library_config.aflags);
            merge(lflags, package.library_config.lflags);
            merge(defines, package.library_config.defines);
            merge(undefines, package.library_config.undefines);
            merge(libs, package.library_config.libs);

            platforms.merge(package.platforms_config);
            compilers.merge(package.compilers_config);

            // Merge shared_ptr dependencies directly
            merge(all_dependencies_array, package.all_dependencies_array);
        }

        Result<void> Build::serialize(toml::value& out) const {
            BaseConfig<Build>::serialize(out);

            out["link"] = muuk::to_string(link_type);

            compilers.serialize(out);
            platforms.serialize(out);

            // Collect dependencies and sort them by (name, version)
            std::vector<std::shared_ptr<Dependency>> sorted_deps;
            for (const auto& dep_ptr : all_dependencies_array)
                if (dep_ptr)
                    sorted_deps.push_back(dep_ptr);

            std::sort(sorted_deps.begin(), sorted_deps.end(), [](const auto& a, const auto& b) {
                if (a->name == b->name)
                    return a->version < b->version;
                return a->name < b->name;
            });

            toml::array dep_array;
            for (const auto& dep_ptr : sorted_deps) {
                toml::value dep_entry;
                TRYV(dep_ptr->serialize(dep_entry));
                dep_array.push_back(dep_entry);
            }

            out["profiles"] = profiles;

            out["dependencies"] = dep_array;
            out.at("dependencies").as_array_fmt().fmt = toml::array_format::multiline;
            return {};
        }

        void Build::load(const toml::value& v, const std::string& base_path) {
            BaseConfig<Build>::load(v, base_path);

            profiles = toml::try_find_or<std::unordered_set<std::string>>(v, "profile", {});

            // Get the link type
            link_type = build_link_from_string(
                toml::try_find_or<std::string>(
                    v,
                    "link",
                    ""));

            // Load dependencies
            for (const auto& [dep_name, versions] : dependencies)
                for (const auto& [dep_version, dep] : versions)
                    all_dependencies_array.insert(std::make_shared<Dependency>(dep));
        }

    } // namespace lockgen
} // namespace muuk
