// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot_manager.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <utility>
#include <sstream>
#include <system_error>

namespace astra {
namespace {
namespace fs = std::filesystem;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool valid_hash(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool safe_value(const std::string& text) {
    return !text.empty() && text.size() <= 256 &&
        std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return c >= 0x21 && c <= 0x7e && c != '=' && c != '#';
        });
}

bool safe_relative_path(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    const std::string str = path.generic_string();
    if (str.size() > 256 || str.find('\\') != std::string::npos ||
        str.find(':') != std::string::npos || str.back() == '/') return false;
    for (const auto& part : path) {
        if (part.empty() || part == "." || part == "..") return false;
    }
    return true;
}

bool parse_manifest(const fs::path& manifest, std::map<std::string, std::string>& fields,
                    std::string& error) {
    std::error_code ec;
    if (fs::is_symlink(manifest, ec) || !fs::is_regular_file(manifest, ec)) {
        error = "Boot plan manifest is missing or is not a regular file";
        return false;
    }
    const auto size = fs::file_size(manifest, ec);
    if (ec || size > 64 * 1024) {
        error = "Manifest is unreadable or exceeds 64 KiB";
        return false;
    }
    std::ifstream input(manifest);
    if (!input) { error = "Cannot open boot plan manifest"; return false; }
    const std::array<std::string, 11> allowed = {
        "format", "target", "loader", "kernel", "kernel_sha256", "initrd",
        "initrd_sha256", "rootfs", "rootfs_sha256", "console", "firmware"
    };
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.size() > 4096) {
            error = "Manifest line exceeds 4096 characters: " + std::to_string(line_number);
            return false;
        }
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            error = "Manifest line has no '=': " + std::to_string(line_number);
            return false;
        }
        const auto key = trim(line.substr(0, eq));
        const auto value = trim(line.substr(eq + 1));
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            error = "Unknown boot plan field: " + key;
            return false;
        }
        if (!safe_value(value)) {
            error = "Invalid value for boot plan field: " + key;
            return false;
        }
        if (!fields.emplace(key, value).second) {
            error = "Duplicate boot plan field: " + key;
            return false;
        }
    }
    if (input.bad()) { error = "Error reading manifest"; return false; }
    for (const auto& required : {"format", "target", "loader", "kernel", "kernel_sha256",
                                 "initrd", "initrd_sha256", "rootfs", "rootfs_sha256"}) {
        if (!fields.count(required)) {
            error = "Missing boot plan field: " + std::string(required);
            return false;
        }
    }
    if (fields["format"] != "astraps5-boot-plan-v1") {
        error = "Expected format=astraps5-boot-plan-v1";
        return false;
    }
    if (fields["target"] == "linux-pc") {
        if (fields["loader"] != "qemu" || fields.count("console") || fields.count("firmware")) {
            error = "linux-pc requires loader=qemu and no console/firmware fields";
            return false;
        }
    } else if (fields["target"] == "ps5-linux") {
        if (fields["loader"] != "ps5-linux-loader" || !fields.count("console") ||
            !fields.count("firmware")) {
            error = "ps5-linux requires ps5-linux-loader, console and firmware";
            return false;
        }
        const auto& model = fields["console"];
        if (model != "ps5" && model != "ps5-slim" && model != "ps5-pro") {
            error = "Unknown PS5 console model";
            return false;
        }
        const auto& fw = fields["firmware"];
        const auto dot = fw.find('.');
        if (dot == std::string::npos || dot == 0 || dot > 2 || fw.size() != dot + 3 ||
            !std::all_of(fw.begin(), fw.end(), [](unsigned char c) {
                return std::isdigit(c) || c == '.';
            })) {
            error = "Firmware must be an explicit version such as 13.60";
            return false;
        }
    } else {
        error = "Unsupported boot target";
        return false;
    }
    for (const auto& name : {"kernel", "initrd", "rootfs"}) {
        if (!safe_relative_path(fs::path(fields[name]))) {
            error = "Invalid relative path for " + std::string(name);
            return false;
        }
        if (!valid_hash(fields[std::string(name) + "_sha256"])) {
            error = "Invalid SHA-256 for " + std::string(name);
            return false;
        }
    }
    return true;
}

// A hash only establishes equality with the manifest; it is not an
// authenticity check. No images are executed by the boot planner.
bool sha256_file(const fs::path& path, std::string& hash, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "Cannot read boot asset"; return false; }
    using MdCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    MdCtx ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        error = "Cannot initialize SHA-256"; return false;
    }
    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto n = input.gcount();
        if (n > 0 && EVP_DigestUpdate(ctx.get(), buffer.data(), static_cast<size_t>(n)) != 1) {
            error = "Cannot update SHA-256"; return false;
        }
    }
    if (input.bad()) { error = "Cannot finish reading boot asset"; return false; }
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &length) != 1 || length != 32) {
        error = "Cannot finalize SHA-256"; return false;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) {
        out << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    hash = out.str();
    return true;
}

bool contained_asset(const fs::path& base, const fs::path& relative,
                     fs::path& resolved, std::string& error) {
    std::error_code ec;
    auto cursor = base;
    for (const auto& component : relative) {
        cursor /= component;
        if (fs::is_symlink(cursor, ec)) {
            error = "Symlinked boot assets are not allowed";
            return false;
        }
        if (ec) { error = "Cannot inspect boot asset path"; return false; }
    }
    resolved = fs::canonical(cursor, ec);
    if (ec || !fs::is_regular_file(resolved)) {
        error = "Boot asset does not exist or is not a regular file";
        return false;
    }
    const auto base_resolved = fs::canonical(base, ec);
    if (ec) { error = "Cannot resolve manifest directory"; return false; }
    auto path_it = resolved.begin();
    for (auto base_it = base_resolved.begin(); base_it != base_resolved.end(); ++base_it, ++path_it) {
        if (path_it == resolved.end() || *base_it != *path_it) {
            error = "Boot asset escapes manifest directory";
            return false;
        }
    }
    if (resolved == base_resolved) { error = "Invalid boot asset path"; return false; }
    return true;
}

std::string json_escape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<unsigned int>(c) << std::dec;
                } else out << static_cast<char>(c);
        }
    }
    return out.str();
}

std::string q(const std::string& s) { return "\"" + json_escape(s) + "\""; }

const char* state_name(BootPlanState state) {
    switch (state) {
        case BootPlanState::ImagesVerified: return "images-verified";
        case BootPlanState::PlanPrepared: return "plan-prepared";
        case BootPlanState::Failed: return "failed";
    }
    return "failed";
}
} // namespace

BootPlan inspect_boot_bundle(const fs::path& manifest) {
    BootPlan plan;
    std::map<std::string, std::string> fields;
    if (!parse_manifest(manifest, fields, plan.error)) return plan;
    plan.target = fields["target"];
    plan.loader = fields["loader"];
    if (fields.count("console")) plan.console = fields["console"];
    if (fields.count("firmware")) plan.firmware = fields["firmware"];
    const auto base = fs::absolute(manifest).parent_path();
    for (const auto& role : {"kernel", "initrd", "rootfs"}) {
        BootAsset asset;
        asset.name = role;
        asset.expected_sha256 = lower(fields[std::string(role) + "_sha256"]);
        if (!contained_asset(base, fs::path(fields[role]), asset.path, plan.error)) {
            plan.error = asset.name + ": " + plan.error;
            return plan;
        }
        std::error_code ec;
        asset.bytes = fs::file_size(asset.path, ec);
        if (ec || asset.bytes == 0) {
            plan.error = asset.name + ": empty or unreadable asset";
            return plan;
        }
        if (!sha256_file(asset.path, asset.actual_sha256, plan.error)) {
            plan.error = asset.name + ": " + plan.error;
            return plan;
        }
        if (asset.actual_sha256 != asset.expected_sha256) {
            plan.error = asset.name + ": SHA-256 mismatch";
            return plan;
        }
        asset.verified = true;
        plan.assets.push_back(std::move(asset));
    }
    plan.images_verified = true;
    plan.state = BootPlanState::ImagesVerified;
    return plan;
}

BootPlan prepare_boot_plan(const fs::path& manifest) {
    BootPlan plan = inspect_boot_bundle(manifest);
    if (!plan.images_verified) return plan;
    plan.steps = {
        "Confirm image publisher/authenticity (signatures not implemented)",
        "Check target hardware and loader compatibility (not implemented)",
        "Provide the verified assets to a compatible boot adapter (not implemented)",
        "Configure a failure-safe recovery path and test on hardware",
        "Boot Astra System and auto-start Astra Shell (not implemented)"
    };
    plan.state = BootPlanState::PlanPrepared;
    return plan;
}

std::string boot_plan_text(const BootPlan& plan) {
    std::ostringstream out;
    out << "Astra Boot Manager v1 (OFFLINE / READ ONLY)\n"
        << "State: " << state_name(plan.state) << '\n';
    if (!plan.error.empty()) out << "Error: " << plan.error << '\n';
    if (!plan.target.empty()) out << "Target: " << plan.target << '\n';
    if (!plan.loader.empty()) out << "Loader: " << plan.loader << '\n';
    if (!plan.console.empty()) out << "Console metadata: " << plan.console << " / " << plan.firmware << '\n';
    for (const auto& asset : plan.assets) {
        out << "  " << asset.name << ": VERIFIED (" << asset.bytes << " bytes)\n";
    }
    if (!plan.steps.empty()) {
        out << "Required next steps:\n";
        for (const auto& step : plan.steps) out << "  - " << step << '\n';
    }
    out << "Bundle images verified: " << (plan.images_verified ? "yes" : "no") << '\n'
        << "Image authenticity: NOT VERIFIED\n"
        << "PS5 hardware compatibility: NOT CHECKED\n"
        << "Boot handoff: NOT IMPLEMENTED\n"
        << "Ready to boot: NO\n";
    return out.str();
}

std::string boot_plan_json(const BootPlan& plan) {
    std::ostringstream out;
    out << "{\n"
        << "  \"state\": " << q(state_name(plan.state)) << ",\n"
        << "  \"error\": " << q(plan.error) << ",\n"
        << "  \"target\": " << q(plan.target) << ",\n"
        << "  \"loader\": " << q(plan.loader) << ",\n"
        << "  \"console\": " << q(plan.console) << ",\n"
        << "  \"firmware\": " << q(plan.firmware) << ",\n"
        << "  \"images_verified\": " << (plan.images_verified ? "true" : "false") << ",\n"
        << "  \"console_checked\": false,\n"
        << "  \"handoff_available\": false,\n"
        << "  \"boot_supported\": false,\n"
        << "  \"assets\": [";
    for (size_t i = 0; i < plan.assets.size(); ++i) {
        const auto& a = plan.assets[i];
        if (i) out << ',';
        out << "\n    {\"role\": " << q(a.name)
            << ", \"path\": " << q(a.path.string())
            << ", \"bytes\": " << a.bytes
            << ", \"sha256\": " << q(a.actual_sha256)
            << ", \"verified\": " << (a.verified ? "true" : "false") << '}';
    }
    if (!plan.assets.empty()) out << '\n';
    out << "  ],\n  \"steps\": [";
    for (size_t i = 0; i < plan.steps.size(); ++i) {
        if (i) out << ", ";
        out << q(plan.steps[i]);
    }
    out << "]\n}\n";
    return out.str();
}

} // namespace astra
