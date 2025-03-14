#include <cctype>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "muukvalidator.hpp"

namespace muuk {
    static const std::unordered_set<char> ALLOWED_CHARS = {
        '-', '_', '/', '.', '+' // Allowed in dependency names
    };

    bool is_valid_dependency_name(std::string_view name) {
        if (name.empty())
            return false;
        if (!std::isalnum(name.front()))
            return false;
        if (!std::isalnum(name.back()) && name.back() != '+')
            return false;

        // Ensure valid characters and no consecutive non-alphanumeric characters
        bool prevNonAlphaNum = false;
        for (std::size_t i = 0; i < name.size(); ++i) {
            char c = name[i];

            if (!std::isalnum(c) && !ALLOWED_CHARS.contains(c)) {
                return false;
            }

            if (!std::isalnum(c)) {
                if (prevNonAlphaNum && c != '+') {
                    return false; // Consecutive non-alphanumeric (except `+`)
                }
                prevNonAlphaNum = true;
            } else {
                prevNonAlphaNum = false;
            }
        }

        // Ensure `.` is wrapped by digits
        for (std::size_t i = 1; i < name.size() - 1; ++i) {
            if (name[i] == '.') {
                if (!std::isdigit(name[i - 1]) || !std::isdigit(name[i + 1])) {
                    return false;
                }
            }
        }

        // Ensure `+` and `/` rules
        std::unordered_map<char, int> charFreq;
        for (char c : name) {
            ++charFreq[c];
        }

        if (charFreq['/'] > 1)
            return false; // At most one `/`
        if (charFreq['+'] != 0 && charFreq['+'] != 2)
            return false; // `+` must be zero or exactly two

        // Ensure `+` characters are consecutive
        if (charFreq['+'] == 2) {
            if (name.find('+') + 1 != name.rfind('+')) {
                return false;
            }
        }

        return true;
    }
}