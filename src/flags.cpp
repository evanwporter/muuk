#include <string>
#include <vector>

#include "compiler.hpp"
#include "flags.hpp"
#include "logger.hpp"
#include "muuk.hpp"
#include "muukvalidator.hpp"

namespace muuk {
    std::string normalize_flag(const std::string& flag, Compiler compiler) {
        std::string normalized_flag = flag;

        if (flag.empty()) {
            muuk::logger::warn("Empty flag provided for {}, returning empty string.", compiler.to_string());
            return flag;
        }

        for (const auto& entry : muuk::flag_table) {
            const auto& [canonical, category, equivalents] = entry;

            const std::string& gcc_flag = std::get<0>(equivalents);
            const std::string& clang_flag = std::get<1>(equivalents);
            const std::string& msvc_flag = std::get<2>(equivalents);

            const std::array<std::string, 3> all_flags = { gcc_flag, clang_flag, msvc_flag };

            for (const auto& known_flag : all_flags) {
                if (!known_flag.empty() && normalized_flag.starts_with(known_flag)) {
                    const std::string& target_flag = [&]() -> const std::string& {
                        switch (compiler.getType()) {
                        case Compiler::Type::GCC:
                            return gcc_flag;
                        case Compiler::Type::Clang:
                            return clang_flag;
                        case Compiler::Type::MSVC:
                            return msvc_flag;
                        }
                        return gcc_flag; // fallback
                    }();

                    return target_flag + normalized_flag.substr(known_flag.length());
                }
            }
        }

        return normalized_flag;
    }

    /// Normalize a vector of flags in-place
    void normalize_flags_inplace(std::vector<std::string>& flags, const Compiler compiler) {
        std::vector<std::string> valid_flags;
        for (auto& flag : flags) {
            flag = normalize_flag(flag, compiler);
            auto validation_result = validate_flag(compiler, flag);
            if (!validation_result) {
                muuk::logger::warn("Skipping invalid flag `{}` for compiler `{}`: {}", flag, compiler.to_string(), validation_result.error());
                continue;
            }
            valid_flags.push_back(flag);
        }
        flags = std::move(valid_flags);
    }
}