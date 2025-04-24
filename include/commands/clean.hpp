#pragma once
#ifndef CLEAN_HPP
#define CLEAN_HPP

#include <toml.hpp>

#include "rustify.hpp"

namespace muuk {
    Result<void> clean(const toml::table& config);
}

#endif // CLEAN_HPP