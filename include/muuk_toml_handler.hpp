#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <toml++/toml.h>

#include "fileops.hpp"
#include "logger.h"
#include "muukvalidator.hpp"
#include "rustify.hpp"
#include "util.h"

namespace muuk {
    class TomlSection {
    public:
        using SectionVariant = std::variant<toml::table, std::vector<toml::table>>;

    private:
        SectionVariant section;

    public:
        TomlSection() = default;
        explicit TomlSection(const toml::table& table) :
            section(table) { }
        explicit TomlSection(const std::vector<toml::table>& tables) :
            section(tables) { }

        toml::table& as_table() { return std::get<toml::table>(section); }
        const toml::table& as_table() const { return std::get<toml::table>(section); }

        std::vector<toml::table>& as_array() { return std::get<std::vector<toml::table>>(section); }
        const std::vector<toml::table>& as_array() const { return std::get<std::vector<toml::table>>(section); }

        template <typename T>
        T& unwrap() { return std::get<T>(section); }

        bool is_table() const { return std::holds_alternative<toml::table>(section); }
        bool is_array() const { return std::holds_alternative<std::vector<toml::table>>(section); }
    };

    class TomlHandler {
    private:
        std::unordered_map<std::string, TomlSection> sections;
        std::vector<std::string> section_order_;
        toml::table content_;

    public:
        static Result<TomlHandler> parse_file(const std::string& file_path) {
            FileOperations file_op(file_path);
            if (!file_op.exists()) {
                return Err("File does not exist: {}", file_path);
            }

            return parse(file_op.read_file());
        }

        static Result<TomlHandler> parse(const std::string& content) {
            TomlHandler handler;

            Try(handler.parse_content(content));

            try {
                handler.content_ = toml::parse(content);
            } catch (const std::exception& e) {
                return Err("Error parsing TOML content: {}", e.what());
            }

            Try(validate_muuk_toml(handler.content_));

            return handler;
        }

        Result<void> parse_content(const std::string& content) {
            std::stringstream file(content);
            std::string line, current_section;
            std::stringstream section_data;
            bool inside_section = false;
            bool is_array_section = false;

            while (std::getline(file, line)) {
                std::string trimmed = util::trim_whitespace(line);

                // Ignore empty lines and comments
                if (trimmed.empty() || trimmed[0] == '#') {
                    continue;
                }

                // Detect arrays of tables [[section]]
                if (trimmed.starts_with("[[") && trimmed.ends_with("]]")) {
                    // Save previous section
                    if (!current_section.empty()) {
                        Try(save_section(current_section, section_data.str(), is_array_section));
                        section_data.str("");
                        section_data.clear();
                    }

                    // Start new section
                    current_section = trimmed.substr(2, trimmed.size() - 4);
                    is_array_section = true;
                    inside_section = true;
                    continue;
                }

                // Detect normal section table [section]
                if (trimmed.starts_with("[") && trimmed.ends_with("]") && !trimmed.starts_with("[[")) {
                    // Save previous section
                    if (!current_section.empty()) {
                        save_section(current_section, section_data.str(), is_array_section);
                        section_data.str("");
                        section_data.clear();
                    }

                    // Start new section
                    current_section = trimmed.substr(1, trimmed.size() - 2);
                    is_array_section = false;
                    inside_section = true;
                    continue;
                }

                // If inside a section, store the lines
                if (inside_section) {
                    section_data << line << "\n";
                }
            }

            // Save the last section after loop ends
            if (!current_section.empty()) {
                save_section(current_section, section_data.str(), is_array_section);
            }

            muuk::logger::info("Final parsed sections:");
            for (const auto& section : section_order_) {
                muuk::logger::info("  - {}", section);
            }

            return {};
        }

        Result<std::reference_wrapper<TomlSection>> get_section(const std::string& name) {
            auto it = sections.find(name);
            if (it == sections.end()) {
                return Err("Section not found: {}", name);
            }
            return it->second;
        }

        std::string serialize() const {
            std::stringstream output;

            for (const auto& section_name : section_order_) {
                const TomlSection& section = sections.at(section_name);

                if (section.is_table()) {
                    // Serialize normal table `[section]`
                    output << "[" << section_name << "]\n";
                    serialize_table(section.as_table(), output);
                } else if (section.is_array()) {
                    // Serialize array of tables `[[section]]`
                    for (auto& table : section.as_array()) {
                        output << "[[" << section_name << "]]\n";
                        serialize_table(table, output);
                    }
                }

                output << "\n";
            }

            return output.str();
        }

        bool has_section(const std::string& name) const {
            return sections.find(name) != sections.end();
        }

        void set_section(const std::string& name, const toml::table& table) {
            auto it = sections.find(name);
            if (it != sections.end()) {
                if (it->second.is_array()) {
                    // If the section is an array, append the new table
                    it->second.as_array().push_back(table);
                } else {
                    // Otherwise, replace the existing section
                    sections[name] = TomlSection(table);
                }
            } else {
                // Create a new section
                sections[name] = TomlSection(table);
                section_order_.push_back(name);
            }
        }

        void set_section(const std::string& name, const std::vector<toml::table>& tables) {
            auto it = sections.find(name);
            if (it != sections.end()) {
                if (it->second.is_array()) {
                    // If it's already an array, extend it
                    it->second.as_array().insert(
                        it->second.as_array().end(), tables.begin(), tables.end());
                } else {
                    muuk::logger::warn("Overwriting existing section with array: {}", name);
                    sections[name] = TomlSection(tables);
                }
            } else {
                // Otherwise, create a new array section
                sections[name] = TomlSection(tables);
                section_order_.push_back(name);
            }
        }

    private:
        Result<void> save_section(const std::string& section_name, const std::string& section_content, bool is_array) {
            try {
                toml::table parsed_table = toml::parse(section_content);

                if (is_array) {
                    if (sections.find(section_name) != sections.end()) {
                        sections[section_name].as_array().push_back(std::move(parsed_table));
                    } else {
                        sections[section_name] = TomlSection(std::vector<toml::table> { std::move(parsed_table) });
                        section_order_.push_back(section_name);
                    }
                } else {
                    if (sections.find(section_name) != sections.end()) {
                        return Err("Duplicate section found: {}", section_name);
                    }
                    sections[section_name] = TomlSection(std::move(parsed_table));
                    section_order_.push_back(section_name);
                }
            } catch (const std::exception& e) { // TOML Parsing Error
                return Err("Error parsing section [{}]: {}", section_name, e.what());
            }

            return {};
        }

        void serialize_table(const toml::table& table, std::stringstream& output, int indent_level = 0) const {
            std::string indent(indent_level * 2, ' '); // Indentation (2 spaces per level)

            for (const auto& [key, value] : table) {
                if (value.is_string()) {
                    output << indent << key << " = " << *value.as_string() << "\n";
                } else if (value.is_integer()) {
                    output << indent << key << " = " << *value.as_integer() << "\n";
                } else if (value.is_floating_point()) {
                    output << indent << key << " = " << *value.as_floating_point() << "\n";
                } else if (value.is_boolean()) {
                    output << indent << key << " = " << (*value.as_boolean() ? "true" : "false") << "\n";
                } else if (value.is_array()) {
                    const auto& arr = *value.as_array();
                    if (arr.empty()) {
                        output << indent << key << " = []\n";
                        continue;
                    }

                    output << indent << key << " = [ ";
                    for (const auto& item : *value.as_array()) {
                        if (item.is_string())
                            output << "" << *item.as_string() << ", ";
                        else if (item.is_integer())
                            output << *item.as_integer() << ", ";
                        else if (item.is_floating_point())
                            output << *item.as_floating_point() << ", ";
                        else if (item.is_boolean())
                            output << (*item.as_boolean() ? "true" : "false") << ", ";
                    }
                    // output.seekp(-2, std::ios_base::end); // Remove trailing comma and space
                    output << " ]\n";
                } else if (value.is_table()) { // Handle inline table `{ ... }`
                    output << indent << key << " = { ";
                    bool first = true;
                    for (const auto& [nested_key, nested_value] : *value.as_table()) {
                        if (!first)
                            output << ", ";
                        first = false;

                        output << nested_key << " = ";

                        if (nested_value.is_string())
                            output << *nested_value.as_string();
                        else if (nested_value.is_integer())
                            output << *nested_value.as_integer();
                        else if (nested_value.is_floating_point())
                            output << *nested_value.as_floating_point();
                        else if (nested_value.is_boolean())
                            output << (*nested_value.as_boolean() ? "true" : "false");
                        else if (nested_value.is_array()) {
                            const auto& nested_arr = *nested_value.as_array();
                            if (nested_arr.empty()) {
                                output << "[]";
                                continue;
                            }

                            output << "[ ";
                            bool first_array = true;
                            for (const auto& array_item : nested_arr) {
                                if (!first_array)
                                    output << ", ";
                                first_array = false;

                                if (array_item.is_string())
                                    output << "" << *array_item.as_string() << "";
                                else if (array_item.is_integer())
                                    output << *array_item.as_integer();
                                else if (array_item.is_floating_point())
                                    output << *array_item.as_floating_point();
                                else if (array_item.is_boolean())
                                    output << (*array_item.as_boolean() ? "true" : "false");
                            }
                            output << " ]";
                        }
                    }
                    output << " }\n";
                }
            }
        }
    };
} // namespace muuk