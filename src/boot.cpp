// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot.hpp"
#include "astraps5/diagnostics.hpp"

#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace astra {
namespace {
namespace fs = std::filesystem;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool valid_hex_hash(const std::string& hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool simple_value(const std::string& text) {
    // Reject unprintable text and separators that would make logs confusing.
    return !text.empty() && text.size() <= 256 &&
        std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return c >= 0x21 && c <= 0x7e && c != '=' && c != '#';
        });
}

bool parse_manifest(const fs::path& path, std::map<std::string, std::string>& values,
                    std::string& error) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        error = "Manifest is missing or is not a regular file";
        return false;
    }
    const auto size = fs::file_size(path, ec);
    if (ec || size > 64 * 1024) {
        error = "Manifest exceeds the 64 KiB limit or cannot be inspected";
        return false;
    }
    std::ifstream input(path);
    if (!input) {
        error = "Unable to open manifest";
        return false;
    }
    const std::array<std::string, 6> keys = { "format", "image", "sha256", "console", "firmware", "loader" };
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.size() > 4096) {
            error = "Manifest line " + std::to_string(line_number) + " exceeds 4096 characters";
            return false;
        }
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            error = "Manifest line " + std::to_string(line_number) + " has no '='";
            return false;
        }
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            error = "Unrecognized manifest key: " + key;
            return false;
        }
        if (!simple_value(value)) {
            error = "Invalid or empty value for manifest key: " + key;
            return false;
        }
        if (!values.emplace(key, value).second) {
            error = "Duplicate manifest key: " + key;
            return false;
        }
    }
    if (input.bad()) {
        error = "Error reading manifest";
        return false;
    }
    for (const auto& required : keys) {
        if (!values.count(required)) {
            error = "Missing required manifest key: " + required;
            return false;
        }
    }
    return true;
}

bool safe_image_path(const fs::path& image) {
    if (image.empty() || image.is_absolute() || image.has_root_name()) return false;
    for (const auto& part : image) {
        if (part == ".." || part == ".") return false;
    }
    // The manifest is currently intentionally limited to simple relative paths.
    return image.string().find('\\') == std::string::npos;
}

bool image_hash_sha256(const fs::path& image, std::string& output, std::string& error) {
    std::ifstream input(image, std::ios::binary);
    if (!input) {
        error = "Cannot open image for read-only hashing";
        return false;
    }
    using DigestPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    DigestPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        error = "SHA-256 initialization failed";
        return false;
    }
    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && EVP_DigestUpdate(context.get(), buffer.data(), static_cast<size_t>(count)) != 1) {
            error = "SHA-256 update failed";
            return false;
        }
    }
    if (input.bad()) {
        error = "Error reading image";
        return false;
    }
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context.get(), digest, &length) != 1 || length != 32) {
        error = "SHA-256 finalization failed";
        return false;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) stream << std::setw(2) << static_cast<unsigned int>(digest[i]);
    output = stream.str();
    return true;
}

std::string json_string(const std::string& value) {
    return "\"" + escape_json(value) + "\"";
}
} // namespace

BootVerification verify_boot_manifest(const std::filesystem::path& manifest_path) {
    BootVerification result;
    std::map<std::string, std::string> manifest;
    if (!parse_manifest(manifest_path, manifest, result.error)) return result;
    if (manifest["format"] != "astraps5-boot-v1") {
        result.error = "Unsupported manifest format (expected astraps5-boot-v1)";
        return result;
    }
    result.console_model = manifest["console"];
    result.firmware = manifest["firmware"];
    result.loader = manifest["loader"];
    result.expected_sha256 = lowercase(manifest["sha256"]);

    if (result.console_model != "ps5" && result.console_model != "ps5-slim" &&
        result.console_model != "ps5-pro") {
        result.error = "Unknown console model. Use ps5, ps5-slim, or ps5-pro";
        return result;
    }
    // No wildcard / range: exact firmware identification is required at this stage.
    const auto dot = result.firmware.find('.');
    if (dot == std::string::npos || dot == 0 || dot > 2 ||
        result.firmware.size() != dot + 3 ||
        std::any_of(result.firmware.begin(), result.firmware.end(), [](unsigned char c) {
            return !std::isdigit(c) && c != '.';
        })) {
        result.error = "Firmware must be an explicit version such as 13.60";
        return result;
    }
    if (result.loader != "ps5-linux-loader") {
        result.error = "Unsupported loader name (expected ps5-linux-loader)";
        return result;
    }
    if (!valid_hex_hash(result.expected_sha256)) {
        result.error = "sha256 must be a 64-character hexadecimal checksum";
        return result;
    }

    const fs::path relative_image = manifest["image"];
    if (!safe_image_path(relative_image)) {
        result.error = "Image must be a relative path without '..' or '.' segments";
        return result;
    }
    const fs::path base = fs::absolute(manifest_path).parent_path();
    result.image_path = (base / relative_image).lexically_normal();
    std::error_code ec;
    if (!fs::is_regular_file(result.image_path, ec)) {
        result.error = "Image not found or is not a regular file";
        return result;
    }
    // Reject symlinks and paths resolving outside the manifest directory to keep
    // the image bundle self-contained and make malicious redirections fail safe.
    const auto canonical_base = fs::canonical(base, ec);
    if (ec) {
        result.error = "Could not resolve manifest directory";
        return result;
    }
    const auto canonical_image = fs::canonical(result.image_path, ec);
    if (ec || canonical_image == canonical_base) {
        result.error = "Could not resolve image path";
        return result;
    }
    auto prefix = canonical_image.begin();
    for (auto base_it = canonical_base.begin(); base_it != canonical_base.end(); ++base_it, ++prefix) {
        if (prefix == canonical_image.end() || *prefix != *base_it) {
            result.error = "Image resolves outside the manifest directory";
            return result;
        }
    }
    result.image_bytes = fs::file_size(result.image_path, ec);
    if (ec || result.image_bytes == 0) {
        result.error = "Cannot read image size or image is empty";
        return result;
    }
    if (!image_hash_sha256(result.image_path, result.actual_sha256, result.error)) return result;
    if (result.actual_sha256 != result.expected_sha256) {
        result.error = "SHA-256 mismatch: image differs from boot manifest";
        return result;
    }
    result.verified = true;
    return result;
}

std::string boot_verification_text(const BootVerification& result) {
    std::ostringstream out;
    out << "Astra Boot Preflight (read-only)\n"
        << "Image integrity: " << (result.verified ? "VERIFIED" : "FAILED") << '\n';
    if (!result.error.empty()) out << "Reason: " << result.error << '\n';
    if (!result.image_path.empty()) out << "Image: " << result.image_path.string() << '\n';
    if (!result.console_model.empty()) out << "Target: " << result.console_model << " / firmware " << result.firmware << '\n';
    if (!result.loader.empty()) out << "Loader: " << result.loader << '\n';
    if (result.verified) out << "Size: " << result.image_bytes << " bytes\n";
    out << "PS5 hardware compatibility: NOT CHECKED\n"
        << "PS5 boot handoff: NOT IMPLEMENTED\n";
    return out.str();
}

std::string boot_verification_json(const BootVerification& result) {
    std::ostringstream out;
    out << "{\n"
        << "  \"image_verified\": " << (result.verified ? "true" : "false") << ",\n"
        << "  \"error\": " << json_string(result.error) << ",\n"
        << "  \"image\": " << json_string(result.image_path.string()) << ",\n"
        << "  \"console\": " << json_string(result.console_model) << ",\n"
        << "  \"firmware\": " << json_string(result.firmware) << ",\n"
        << "  \"loader\": " << json_string(result.loader) << ",\n"
        << "  \"image_bytes\": " << result.image_bytes << ",\n"
        << "  \"expected_sha256\": " << json_string(result.expected_sha256) << ",\n"
        << "  \"actual_sha256\": " << json_string(result.actual_sha256) << ",\n"
        << "  \"console_checked\": false,\n"
        << "  \"boot_supported\": false\n"
        << "}\n";
    return out.str();
}

} // namespace astra
