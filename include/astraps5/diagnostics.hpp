// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace astra {

struct Check {
    std::string id;
    std::string status; // ok, warning, or info
    std::string detail;
};

struct Report {
    std::vector<Check> checks;
};

std::string escape_json(std::string_view text);
std::string extract_vulkan_device(std::string_view output);
bool contains_ps5_marker(std::string_view text);
Report collect_diagnostics();
std::string report_json(const Report& report);
std::string report_text(const Report& report);

} // namespace astra
