#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include <type_traits>

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {

    Result<toml::value> parse_muuk_file(const std::string& path, bool is_lockfile = false);

    std::vector<std::string> parse_array_as_vec(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix = "");

    std::unordered_set<std::string> parse_array_as_set(
        const toml::value& table,
        const std::string& key,
        const std::string& prefix = "");

    template <typename T>
    T find_or(const toml::value& v, const std::string& key, const T& default_val) {
        if (!v.contains(key)) return default_val;
        return toml::find_or<T>(v, key, default_val);
    }
    
    // Specialization for std::unordered_set<T>
    template <typename T>
    std::unordered_set<T> find_or(const toml::value& v, const std::string& key, const std::unordered_set<T>& default_val) {
        std::vector<T> default_vec(default_val.begin(), default_val.end());
        std::vector<T> vec = toml::find_or(v, key, default_vec);
        return std::unordered_set<T>(vec.begin(), vec.end());
    }
} // namespace muuk
