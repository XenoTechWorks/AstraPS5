// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot_manager.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

int main() {
    char directory[] = "/tmp/astra-boot-manager-XXXXXX";
    const char* temp = mkdtemp(directory);
    assert(temp);
    const fs::path base(temp);
    struct Cleanup { fs::path root; ~Cleanup() { std::error_code ec; fs::remove_all(root, ec); } } cleanup{base};
    fs::create_directories(base / "boot");
    fs::create_directories(base / "system");
    const std::string digest = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    for (const auto& path : {"boot/vmlinuz", "boot/initrd.img", "system/rootfs.img"}) {
        std::ofstream(base / path, std::ios::binary) << "hello";
    }
    const fs::path manifest = base / "astra.bootplan";
    const auto write = [&](const std::string& target = "linux-pc", const std::string& override = "",
                           const std::string& extra = "") {
        std::ofstream out(manifest);
        out << "format=astraps5-boot-plan-v1\n"
            << "target=" << target << "\n"
            << "loader=" << (target == "linux-pc" ? "qemu" : "ps5-linux-loader") << "\n"
            << "kernel=boot/vmlinuz\n"
            << "kernel_sha256=" << digest << "\n"
            << "initrd=boot/initrd.img\n"
            << "initrd_sha256=" << digest << "\n"
            << "rootfs=" << (override.empty() ? "system/rootfs.img" : override) << "\n"
            << "rootfs_sha256=" << digest << "\n";
        if (target == "ps5-linux") out << "console=ps5\nfirmware=13.60\n";
        out << extra;
    };
    write();
    {
        const auto result = astra::inspect_boot_bundle(manifest);
        assert(result.images_verified);
        assert(result.state == astra::BootPlanState::ImagesVerified);
        assert(result.assets.size() == 3);
        assert(result.steps.empty());
        assert(!result.console_checked && !result.handoff_available && !result.boot_supported);
        const auto json = astra::boot_plan_json(result);
        assert(json.find("\"images_verified\": true") != std::string::npos);
        assert(json.find("\"boot_supported\": false") != std::string::npos);
    }
    {
        const auto result = astra::prepare_boot_plan(manifest);
        assert(result.state == astra::BootPlanState::PlanPrepared);
        assert(result.steps.size() == 5);
        assert(astra::boot_plan_text(result).find("Ready to boot: NO") != std::string::npos);
    }
    write("ps5-linux");
    {
        const auto result = astra::prepare_boot_plan(manifest);
        assert(result.images_verified && result.target == "ps5-linux");
        assert(result.console == "ps5" && result.firmware == "13.60");
        assert(!result.boot_supported && !result.console_checked);
    }
    write("linux-pc", "../outside.img");
    assert(astra::inspect_boot_bundle(manifest).error.find("relative path") != std::string::npos);
    write("linux-pc", "/tmp/outside.img");
    assert(astra::inspect_boot_bundle(manifest).error.find("relative path") != std::string::npos);
    write("linux-pc", "system/rootfs.img", "loader=evil\n");
    assert(astra::inspect_boot_bundle(manifest).error.find("Duplicate") != std::string::npos);
    write("linux-pc", "system/rootfs.img", "unknown=data\n");
    assert(astra::inspect_boot_bundle(manifest).error.find("Unknown") != std::string::npos);
    write("linux-pc", "system/rootfs.img", "console=ps5\n");
    assert(astra::inspect_boot_bundle(manifest).error.find("no console") != std::string::npos);
    fs::rename(base / "system/rootfs.img", base / "system/rootfs-original.img");
    fs::create_symlink("rootfs-original.img", base / "system/rootfs.img");
    write();
    assert(astra::inspect_boot_bundle(manifest).error.find("Symlinked") != std::string::npos);
    fs::remove(base / "system/rootfs.img");
    fs::rename(base / "system/rootfs-original.img", base / "system/rootfs.img");
    std::ofstream(base / "boot/vmlinuz") << "changed";
    assert(astra::inspect_boot_bundle(manifest).error.find("SHA-256 mismatch") != std::string::npos);
    fs::remove(base / "boot/initrd.img");
    std::ofstream(base / "boot/vmlinuz") << "hello";
    assert(!astra::prepare_boot_plan(manifest).images_verified);
    assert(!astra::inspect_boot_bundle(base / "missing.boot").images_verified);
    std::cout << "Astra Boot Manager v1 unit tests passed.\n";
}
