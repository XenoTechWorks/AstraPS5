// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/diagnostics.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

int main() {
    assert(astra::escape_json("a\"b\\c\n") == "a\\\"b\\\\c\\n");
    assert(astra::escape_json(std::string(1, '\x01')) == "\\u0001");
    assert(astra::extract_vulkan_device("GPU0:\n deviceName = AMD Radeon Graphics\n") == "AMD Radeon Graphics");
    assert(astra::extract_vulkan_device("nothing here").empty());
    assert(astra::contains_ps5_marker("Linux ps5-linux-patches"));
    assert(astra::contains_ps5_marker("Sony PlayStation 5"));
    assert(!astra::contains_ps5_marker("generic desktop PC"));
    astra::Report report{{{"test", "ok", "escaped \"item\""}}};
    auto json = astra::report_json(report);
    assert(json.find("escaped \\\"item\\\"") != std::string::npos);
    std::cout << "Unit tests passed.\n";
}
