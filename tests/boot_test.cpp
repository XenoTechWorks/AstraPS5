// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

int main() {
    char directory[] = "/tmp/astra-boot-tests-XXXXXX";
    char* name = mkdtemp(directory);
    assert(name);
    const fs::path base(name);
    const fs::path manifest = base / "astraps5.boot";
    const std::string valid_hash = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); }
    } cleanup{base};
    {
        std::ofstream image(base / "astra.img", std::ios::binary);
        image << "hello";
    }
    const auto write_manifest = [&](const std::string& extra = "", const std::string& hash = "") {
        std::ofstream file(manifest);
        file << "# Astra test manifest\n"
             << "format=astraps5-boot-v1\n"
             << "image=astra.img\n"
             << "sha256=" << (hash.empty() ? valid_hash : hash) << "\n"
             << "console=ps5\n"
             << "firmware=13.60\n"
             << "loader=ps5-linux-loader\n"
             << extra;
    };
    write_manifest();
    {
        auto result = astra::verify_boot_manifest(manifest);
        assert(result.verified);
        assert(result.image_bytes == 5);
        assert(result.actual_sha256 == valid_hash);
        assert(result.error.empty());
        assert(astra::boot_verification_json(result).find("\"image_verified\": true") != std::string::npos);
        assert(astra::boot_verification_text(result).find("PS5 boot handoff: NOT IMPLEMENTED") != std::string::npos);
    }
    write_manifest("", std::string(64, '0'));
    assert(astra::verify_boot_manifest(manifest).error.find("mismatch") != std::string::npos);
    write_manifest("", "bad-checksum");
    assert(astra::verify_boot_manifest(manifest).error.find("64-character") != std::string::npos);
    write_manifest("firmware=12.00\n");
    assert(astra::verify_boot_manifest(manifest).error.find("Duplicate") != std::string::npos);
    write_manifest("unexpected=value\n");
    assert(astra::verify_boot_manifest(manifest).error.find("Unrecognized") != std::string::npos);
    write_manifest();
    {
        std::ofstream file(manifest);
        file << "format=astraps5-boot-v1\nimage=../outside.img\n"
             << "sha256=" << valid_hash << "\nconsole=ps5\nfirmware=13.60\nloader=ps5-linux-loader\n";
    }
    assert(astra::verify_boot_manifest(manifest).error.find("relative") != std::string::npos);
    {
        std::ofstream file(manifest);
        file << "format=astraps5-boot-v1\nimage=astra.img\n"
             << "sha256=" << valid_hash << "\nconsole=ps5-ultra\nfirmware=13.60\nloader=ps5-linux-loader\n";
    }
    assert(astra::verify_boot_manifest(manifest).error.find("Unknown console") != std::string::npos);
    // Reject malformed firmware and verify a file with mixed-case expected hash.
    {
        std::ofstream file(manifest);
        file << "format=astraps5-boot-v1\nimage=astra.img\n"
             << "sha256=" << valid_hash << "\nconsole=ps5\nfirmware=13..60\nloader=ps5-linux-loader\n";
    }
    assert(astra::verify_boot_manifest(manifest).error.find("Firmware") != std::string::npos);
    // Reject missing required fields before touching the image.
    {
        std::ofstream file(manifest);
        file << "format=astraps5-boot-v1\nimage=astra.img\n";
    }
    assert(astra::verify_boot_manifest(manifest).error.find("Missing required") != std::string::npos);
    write_manifest();
    fs::remove(base / "astra.img");
    assert(astra::verify_boot_manifest(manifest).error.find("not found") != std::string::npos);
    assert(!astra::verify_boot_manifest(base / "nonexistent.conf").verified);
    std::cout << "Boot preflight unit tests passed.\n";
}
