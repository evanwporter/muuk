// #pragma once

// #include <sstream>
// #include <string>
// #include <unordered_map>
// #include <variant>
// #include <vector>

// #include <toml++/toml.hpp>

// #include "rustify.hpp"

// namespace muuk {
//     class TomlSection {
//     public:
//         using SectionVariant = std::variant<toml::table, std::vector<toml::table>>;

//     private:
//         SectionVariant section;

//     public:
//         TomlSection() = default;
//         explicit TomlSection(const toml::table& table) :
//             section(table) { }
//         explicit TomlSection(const std::vector<toml::table>& tables) :
//             section(tables) { }

//         toml::table& as_table() { return std::get<toml::table>(section); }
//         const toml::table& as_table() const { return std::get<toml::table>(section); }

//         std::vector<toml::table>& as_array() { return std::get<std::vector<toml::table>>(section); }
//         const std::vector<toml::table>& as_array() const { return std::get<std::vector<toml::table>>(section); }

//         template <typename T>
//         T& unwrap() { return std::get<T>(section); }

//         bool is_table() const { return std::holds_alternative<toml::table>(section); }
//         bool is_array() const { return std::holds_alternative<std::vector<toml::table>>(section); }
//     };

//     class TomlHandler {
//     private:
//         std::unordered_map<std::string, TomlSection> sections;
//         std::vector<std::string> section_order_;
//         toml::table content_;

//     public:
//         static Result<TomlHandler> parse_file(const std::string& file_path);

//         static Result<TomlHandler> parse(const std::string& content);

//         Result<void> parse_content(const std::string& content);

//         Result<std::reference_wrapper<TomlSection>> get_section(const std::string& name);

//         std::string serialize() const;

//         bool has_section(const std::string& name) const;

//         void set_section(const std::string& name, const toml::table& table);

//         void set_section(const std::string& name, const std::vector<toml::table>& tables);

//     private:
//         Result<void> save_section(const std::string& section_name, const std::string& section_content, bool is_array);

//         void serialize_table(const toml::table& table, std::stringstream& output, int indent_level = 0) const;
//     };
// } // namespace muuk