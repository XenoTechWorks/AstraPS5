// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace astra {

// Verifies a boot IMAGE's integrity, NOT PS5 firmware support or console readiness.
// There is deliberately no function here that boots a device or writes to storage.
struct BootVerification {
    bool verified = false;
    std::string error;
    std::filesystem::path image_path;
    std::string console_model;
    std::string firmware;
    std::string loader;
    std::string expected_sha256;
    std::string actual_sha256;
    std::uintmax_t image_bytes = 0;
};

// Boot manifest format v1: strict key=value, located beside the intended image.
// Required: format, image, sha256, console, firmware, loader.
BootVerification verify_boot_manifest(const std::filesystem::path& manifest_path);
std::string boot_verification_text(const BootVerification& result);
std::string boot_verification_json(const BootVerification& result);

} // namespace astra
