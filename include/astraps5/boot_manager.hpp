// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace astra {

// Offline preparation only. No state here implies a console was inspected,
// a loader was executed, or an operating system has booted.
enum class BootPlanState {
    Failed,
    ImagesVerified,
    PlanPrepared
};

struct BootAsset {
    std::string name;
    std::filesystem::path path;
    std::string expected_sha256;
    std::string actual_sha256;
    std::uintmax_t bytes = 0;
    bool verified = false;
};

struct BootPlan {
    BootPlanState state = BootPlanState::Failed;
    std::string error;
    std::string target;
    std::string loader;
    std::string console;
    std::string firmware;
    std::vector<BootAsset> assets;
    std::vector<std::string> steps;
    bool images_verified = false;
    // The following stay false until PS5 hardware and loader support exists.
    bool console_checked = false;
    bool handoff_available = false;
    bool boot_supported = false;
};

// v2 plans do not change the existing 'astra boot verify' v1 behavior.
// Both functions read files only and never attempt any boot transition.
BootPlan inspect_boot_bundle(const std::filesystem::path& manifest);
BootPlan prepare_boot_plan(const std::filesystem::path& manifest);
std::string boot_plan_text(const BootPlan& plan);
std::string boot_plan_json(const BootPlan& plan);

} // namespace astra
