/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "logger.hpp"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  Logger::init("smf_unit_tests", false, false);

  return RUN_ALL_TESTS();
}
