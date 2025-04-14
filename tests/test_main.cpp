//===-- test_main.cpp - Test entry point ------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License
//
//===----------------------------------------------------------------------===//
//
// \file
// Nothing but a placeholder.
//
//===----------------------------------------------------------------------===//

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>

namespace sockify {
void hello();
} // namespace sockify

TEST_CASE("sockify::hello outputs the correct greeting", "[hello]")
{
  std::stringstream sstr;
  std::streambuf* buf = std::cout.rdbuf(sstr.rdbuf());

  sockify::hello();

  std::cout.rdbuf(buf);
  REQUIRE(sstr.str() == "Hello Sockify!\n");
}
