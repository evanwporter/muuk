#ifndef MUUK_FILER_H
#define MUUK_FILER_H

#include <unordered_map>
#include <vector>
#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>
#include <tl/expected.hpp>

#include "ifileops.hpp"
#include "fileops.hpp"
#include "rustify.hpp"
#include "types.h"

class MuukFiler {
private:
    std::shared_ptr<IFileOperations> file_ops_;
    std::string config_file_;
    std::unordered_map<std::string, toml::table> sections_;
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

    const std::unordered_map<std::string, toml::table>& get_sections() const {
        return sections_;
    }

    static Result<MuukFiler> create(std::shared_ptr<IFileOperations> file_ops, bool is_lock_file = false);

    static Result<MuukFiler> create(const std::string& config_file, bool is_lock_file = false);

private:

    void parse();
};

#endif // MUUK_FILER_H
