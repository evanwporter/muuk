#pragma once
#ifndef CLEAN_HPP
#define CLEAN_HPP

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> clean(const toml::ordered_table& config);
}

#endif // CLEAN_HPP