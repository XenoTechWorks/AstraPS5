// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/menu.hpp"
#include <iostream>
namespace astra {
int run_menu() {
    std::cerr << "The native Astra menu needs X11 development libraries at build time.\n"
                 "On Debian/Ubuntu: sudo apt install libx11-dev\n"
                 "Then rebuild with CMake.\n";
    return 1;
}
} // namespace astra
