#ifndef MUUK_FILER_H
#define MUUK_FILER_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <tl/expected.hpp>
#include <toml++/toml.hpp> // TODO: USE TOML11

#include "ifileops.hpp"
#include "rustify.hpp"
#include "types.h"

class MuukFiler {
private:
    std::shared_ptr<IFileOperations> file_ops_;
    std::string config_file_;
    std::unordered_map<std::string, toml::table> sections_;
    std::unordered_map<std::string, std::vector<toml::table>> array_sections_;

    std::vector<std::string> section_order_;

    toml::table config_;

    bool is_lock_file;

public:
    virtual ~MuukFiler() = default;

    explicit MuukFiler(std::shared_ptr<IFileOperations> file_ops, bool is_lock_file = false);
    explicit MuukFiler(const std::string& config_file, bool is_lock_file = false);

    toml::table& get_section(const std::string& section);
    void modify_section(const std::string& section, const toml::table& data);
    void write_to_file();

    toml::table get_config() const;
    bool has_section(const std::string& section) const;

    const std::vector<std::string>& get_section_order() const {
        return section_order_;
    }

    const std::unordered_map<std::string, toml::table>&
    get_sections() const {
        return sections_;
    }

    const std::unordered_map<std::string, std::vector<toml::table>>&
    get_array_section() const {
        return array_sections_;
    }

    std::unordered_map<std::string, toml::table>& get_build_section() {
        return build_sections_;
    }

    std::unordered_map<std::string, std::unordered_map<std::string, toml::table>>& get_library_section() {
        return library_sections_;
    }

    std::optional<toml::table> get_library_section(const std::string& library_name, const std::string& version) {
        if (library_sections_.find(library_name) != library_sections_.end()) {
            auto& versions = library_sections_.at(library_name);
            if (versions.find(version) != versions.end()) {
                return versions.at(version);
            }
        }
        return std::nullopt;
    }

    static Result<MuukFiler> create(std::shared_ptr<IFileOperations> file_ops, bool is_lock_file = false);

    static Result<MuukFiler> create(const std::string& config_file, bool is_lock_file = false);

    static std::vector<std::string> parse_array_as_vec(const toml::table& table, const std::string& key, const std::string& prefix = "");
    static std::unordered_set<std::string> parse_array_as_set(const toml::table& table, const std::string& key, const std::string& prefix = "");

private:
    void parse();
    std::unordered_map<std::string, toml::table> build_sections_;
    DependencyVersionMap<toml::table> library_sections_;
};

#endif // MUUK_FILER_H
