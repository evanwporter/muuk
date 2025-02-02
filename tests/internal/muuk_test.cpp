#include <gtest/gtest.h>
#include "muuk_test.hpp"
#include "util_test.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

