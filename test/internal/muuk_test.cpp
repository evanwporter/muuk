#include <gtest/gtest.h>

#include "test_build_manager.hpp"
#include "test_buildparser.hpp"
#include "test_muukfiler.hpp"
#include "test_muukvalidator.hpp"
#include "test_toml_handler.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
