#ifndef MUUK_FILER_H
#define MUUK_FILER_H

#include <toml++/toml.hpp>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

class IMuukFiler {
public:
    virtual ~IMuukFiler() = default;

    virtual const toml::table& get_config() const = 0;
    virtual void set_config(const toml::table& new_config) = 0;
    virtual bool has_section(const std::string& section) const = 0;
    virtual toml::table get_section(const std::string& section) const = 0;
    virtual void update_section(const std::string& section, const toml::table& data) = 0;

    virtual void set_value(const std::string& section, const std::string& key, const toml::node& value) = 0;
    virtual void remove_key(const std::string& section, const std::string& key) = 0;
    virtual void remove_section(const std::string& section) = 0;
    virtual void append_to_array(const std::string& section, const std::string& key, const toml::node& value) = 0;
};

class MuukFiler : public IMuukFiler {
public:
    explicit MuukFiler(const std::string& config_file);

    const toml::table& get_config() const;
    toml::table get_section(const std::string& section) const;

    void set_config(const toml::table& new_config);
    void update_section(const std::string& section, const toml::table& data);
    void set_value(const std::string& section, const std::string& key, const toml::node& value);
    void remove_key(const std::string& section, const std::string& key);
    void remove_section(const std::string& section);
    void append_to_array(const std::string& section, const std::string& key, const toml::node& value);

    void load_config();

    bool has_section(const std::string& section) const;

    std::vector<std::string> parse_section_order();

private:
    std::string config_file_;
    toml::table config_;
    std::vector<std::string> section_order_;
    std::shared_ptr<spdlog::logger> logger_;

    void load_or_create_config();
    void save_config();
    void ensure_section_exists(const std::string& section);
    void track_section_order(const std::string& section = "");
    std::string format_error(const std::string& error_message);

    void validate_muuk();
    void validate_array_of_strings(const toml::table& section, const std::string& key, bool required = false);

    toml::table get_default_config() const;
};

class MockMuukFiler : public IMuukFiler {
public:
    MockMuukFiler();

    const toml::table& get_config() const override;
    void set_config(const toml::table& new_config) override;
    bool has_section(const std::string& section) const override;
    toml::table get_section(const std::string& section) const override;
    void update_section(const std::string& section, const toml::table& data) override;

    void set_value(const std::string& section, const std::string& key, const toml::node& value) override;
    void remove_key(const std::string& section, const std::string& key) override;
    void remove_section(const std::string& section) override;
    void append_to_array(const std::string& section, const std::string& key, const toml::node& value) override;

private:
    toml::table config_;
};

#endif // MUUK_FILER_H
