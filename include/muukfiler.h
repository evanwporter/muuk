#ifndef MUUKFILER_HPP
#define MUUKFILER_HPP

#include <string>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>
#include "../include/toml.hpp"

class IMuukFiler {
public:
    virtual ~IMuukFiler() = default;

    virtual const toml::table& get_config() const = 0;
    virtual void set_config(const toml::table& new_config) = 0;
    virtual bool has_section(const std::string& section) const = 0;
    virtual toml::table get_section(const std::string& section) const = 0;
    virtual void update_section(const std::string& section, const toml::table& data) = 0;

    // **New Functionalities**
    virtual void set_value(const std::string& section, const std::string& key, const toml::node& value) = 0;
    virtual void remove_key(const std::string& section, const std::string& key) = 0;
    virtual void remove_section(const std::string& section) = 0;
    virtual void append_to_array(const std::string& section, const std::string& key, const toml::node& value) = 0;
};

class MuukFiler : public IMuukFiler {
public:
    explicit MuukFiler(const std::string& config_file);

    const toml::table& get_config() const override;
    void set_config(const toml::table& new_config) override;

    bool has_section(const std::string& section) const override;
    toml::table get_section(const std::string& section) const override;
    void update_section(const std::string& section, const toml::table& data) override;

    // **New Functionalities**
    void set_value(const std::string& section, const std::string& key, const toml::node& value) override;
    void remove_key(const std::string& section, const std::string& key) override;
    void remove_section(const std::string& section) override;
    void append_to_array(const std::string& section, const std::string& key, const toml::node& value) override;

private:
    std::string config_file_;
    toml::table config_;
    std::vector<std::string> section_order_;
    std::shared_ptr<spdlog::logger> logger_;

    toml::table load_or_create_config();
    void save_config();
    toml::table get_default_config() const;
    void track_section_order();
};

// Mock Class for Testing
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

#endif // MUUKFILER_HPP
