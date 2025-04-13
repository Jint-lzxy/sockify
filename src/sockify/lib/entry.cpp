//===-- entry.cpp - Library entry point -------------------------*- C++ -*-===//
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

#ifdef _WIN32
#  define DLLEXPORT __declspec(dllexport)
#else
#  define DLLEXPORT
#endif

#include <iostream>

namespace sockify {
DLLEXPORT void hello() { std::cout << "Hello Sockify!\n"; }
} // namespace sockify
