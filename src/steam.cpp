// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
// Local Steam KeyValues discovery. No shell, network, executable launch,
// game downloads, Steam authentication, or direct Proton execution.
#include "astraps5/steam.hpp"
#include "astraps5/diagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace astra {
namespace {
constexpr std::uintmax_t kMaxVdfBytes = 2 * 1024 * 1024;
constexpr std::size_t kMaxVdfNodes = 25000;
constexpr std::size_t kMaxLibraries = 32;
constexpr std::size_t kMaxEntriesPerLibrary = 4096;
constexpr std::size_t kMaxGames = 500;
constexpr std::size_t kMaxTools = 80;

struct VdfNode {
    std::string text;
    std::map<std::string, VdfNode> children;
    bool object = false;
};

std::string lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool safe_text(const std::string& s, std::size_t max) {
    if (s.empty() || s.size() > max) return false;
    for (unsigned char c : s) if (c < 32 || c == 127) return false;
    return true;
}

bool digits_only(const std::string& s) {
    if (s.empty() || s.size() > 12) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}

std::string value(const VdfNode& node, const std::string& key) {
    const auto it = node.children.find(lower(key));
    if (it == node.children.end() || it->second.object) return {};
    return it->second.text;
}

const VdfNode* child(const VdfNode& node, const std::string& key) {
    const auto it = node.children.find(lower(key));
    return it == node.children.end() || !it->second.object ? nullptr : &it->second;
}

// Small bounded lexer/parser for Valve's text KeyValues1 format.
// Not compatible with binary appinfo.vdf/shortcuts.vdf (intentionally).
class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}
    bool parse(VdfNode& output) {
        output.object = true;
        return block(output, false, 0) && !bad_;
    }
private:
    std::string_view text_;
    std::size_t at_ = 0;
    std::size_t count_ = 0;
    bool bad_ = false;

    void whitespace() {
        for (;;) {
            while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_]))) ++at_;
            if (at_ + 1 < text_.size() && text_[at_] == '/' && text_[at_ + 1] == '/') {
                at_ += 2;
                while (at_ < text_.size() && text_[at_] != '\n') ++at_;
            } else break;
        }
    }
    bool token(std::string& out) {
        whitespace();
        out.clear();
        if (at_ == text_.size()) return false;
        if (text_[at_] == '{' || text_[at_] == '}') {
            out.push_back(text_[at_++]);
            return true;
        }
        if (text_[at_] == '"') {
            ++at_;
            bool closed = false;
            while (at_ < text_.size()) {
                char c = text_[at_++];
                if (c == '"') { closed = true; break; }
                if (c == '\\' && at_ < text_.size()) {
                    const char next = text_[at_];
                    if (next == '\\' || next == '"') { c = next; ++at_; }
                }
                if (c == '\0' || out.size() >= 8192) { bad_ = true; return false; }
                out.push_back(c);
            }
            if (!closed) bad_ = true;
            return closed;
        }
        while (at_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[at_])) &&
               text_[at_] != '{' && text_[at_] != '}') {
            if (out.size() >= 8192 || text_[at_] == '\0') { bad_ = true; return false; }
            out.push_back(text_[at_++]);
        }
        return !out.empty();
    }
    bool block(VdfNode& parent, bool nested, unsigned depth) {
        if (depth > 12) return false;
        std::string key, next;
        for (;;) {
            if (!token(key)) return !nested && !bad_ && at_ == text_.size();
            if (key == "}") return nested;
            if (key == "{" || ++count_ > kMaxVdfNodes) return false;
            if (!token(next) || next == "}") return false;
            VdfNode item;
            if (next == "{") {
                item.object = true;
                if (!block(item, true, depth + 1)) return false;
            } else {
                item.text = std::move(next);
            }
            parent.children[lower(key)] = std::move(item);
        }
    }
};

bool read_vdf(const fs::path& file, VdfNode& node) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || ec) return false;
    const auto bytes = fs::file_size(file, ec);
    if (ec || bytes == 0 || bytes > kMaxVdfBytes) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.eof() && in.fail()) return false;
    if (contents.size() != bytes) return false;
    return Parser(contents).parse(node);
}

bool add_library(SteamDiscovery& result, std::set<fs::path>& visited, const fs::path& candidate) {
    if (result.libraries.size() >= kMaxLibraries || !candidate.is_absolute()) return false;
    std::error_code ec;
    const auto normalized = fs::weakly_canonical(candidate, ec);
    if (ec || !fs::is_directory(normalized / "steamapps", ec) || ec) return false;
    if (!visited.insert(normalized).second) return false;
    result.libraries.push_back(normalized);
    return true;
}

void add_libraries_from_vdf(SteamDiscovery& result, std::set<fs::path>& visited, const fs::path& file) {
    VdfNode fileRoot;
    if (!read_vdf(file, fileRoot)) return;
    const auto* folders = child(fileRoot, "libraryfolders");
    if (!folders) return;
    for (const auto& [index, entry] : folders->children) {
        if (!digits_only(index)) continue;
        const std::string path = entry.object ? value(entry, "path") : entry.text;
        if (!path.empty() && path.size() <= 4096) add_library(result, visited, fs::path(path));
    }
}

bool is_tool_name(std::string name) {
    name = lower(std::move(name));
    return name == "proton" || name.rfind("proton ", 0) == 0 ||
           name.rfind("proton-", 0) == 0 || name.rfind("ge-proton", 0) == 0 ||
           name.rfind("steam linux runtime", 0) == 0 ||
           name == "steamworks common redistributables" || name == "steam";
}

void inspect_library(SteamDiscovery& result, const fs::path& library) {
    std::error_code ec;
    const fs::path apps = library / "steamapps";
    fs::directory_iterator it(apps, fs::directory_options::skip_permission_denied, ec);
    const fs::directory_iterator end;
    std::size_t count = 0;
    for (; it != end && !ec && count < kMaxEntriesPerLibrary; it.increment(ec), ++count) {
        const auto file = it->path();
        if (!fs::is_regular_file(file, ec) || ec) { ec.clear(); continue; }
        const auto fileName = file.filename().string();
        if (fileName.rfind("appmanifest_", 0) != 0 || file.extension() != ".acf") continue;
        const std::string fileId = fileName.substr(12, fileName.size() - 16);
        if (!digits_only(fileId)) continue;
        VdfNode root;
        if (!read_vdf(file, root)) continue;
        const auto* state = child(root, "AppState");
        if (!state || value(*state, "appid") != fileId) continue;
        const std::string name = value(*state, "name");
        const std::string installDir = value(*state, "installdir");
        const std::string flags = value(*state, "StateFlags");
        if (!safe_text(name, 120) || !safe_text(installDir, 180) || is_tool_name(name)) continue;
        // Bit 2 means "Fully Installed"; manifests alone may be stale.
        std::uint64_t status = 0;
        if (!digits_only(flags)) continue;
        try { status = std::stoull(flags); } catch (...) { continue; }
        if ((status & 4) == 0) continue;
        // A single directory component; do not follow manifest paths outside common.
        if (installDir == "." || installDir == ".." || installDir.find('/') != std::string::npos ||
            installDir.find('\\') != std::string::npos) continue;
        const fs::path installed = apps / "common" / installDir;
        if (!fs::is_directory(installed, ec) || ec) { ec.clear(); continue; }
        if (result.games.size() < kMaxGames) result.games.push_back({fileId, name, installed});
    }
}

void inspect_proton_directory(SteamDiscovery& result, const fs::path& path, bool custom) {
    if (result.protons.size() >= kMaxTools) return;
    std::error_code ec;
    if (!fs::is_directory(path, ec) || ec) return;
    const std::string name = path.filename().string();
    if (!custom && !is_tool_name(name)) return;
    if (custom && !fs::is_regular_file(path / "compatibilitytool.vdf", ec)) return;
    if (!fs::is_regular_file(path / "proton", ec) || ec) return;
    result.protons.push_back({name, path, custom});
}

void scan_proton_dir(SteamDiscovery& result, const fs::path& dir, bool custom) {
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    const fs::directory_iterator end;
    std::size_t count = 0;
    for (; it != end && !ec && count < 256 && result.protons.size() < kMaxTools; it.increment(ec), ++count)
        inspect_proton_directory(result, it->path(), custom);
}

} // namespace

SteamDiscovery discover_steam_in(const std::vector<fs::path>& steam_roots) {
    SteamDiscovery result;
    std::set<fs::path> visited;
    std::set<fs::path> roots;
    for (const auto& raw : steam_roots) {
        if (!raw.is_absolute()) continue;
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(raw, ec);
        if (ec || !fs::is_directory(root / "steamapps", ec) || ec || !roots.insert(root).second) continue;
        result.steam_directory_found = true;
        add_library(result, visited, root);
        add_libraries_from_vdf(result, visited, root / "steamapps/libraryfolders.vdf");
        add_libraries_from_vdf(result, visited, root / "config/libraryfolders.vdf");
        scan_proton_dir(result, root / "compatibilitytools.d", true);
    }
    for (const auto& library : result.libraries) {
        inspect_library(result, library);
        scan_proton_dir(result, library / "steamapps/common", false);
    }
    std::sort(result.games.begin(), result.games.end(), [](const auto& a, const auto& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.app_id < b.app_id;
    });
    // Multiple Steam roots can reference the same game; prefer one entry per ID.
    std::set<std::string> ids;
    result.games.erase(std::remove_if(result.games.begin(), result.games.end(), [&](const auto& game) {
        return !ids.insert(game.app_id).second;
    }), result.games.end());
    std::sort(result.protons.begin(), result.protons.end(), [](const auto& a, const auto& b) {
        return a.name < b.name || (a.name == b.name && a.directory < b.directory);
    });
    result.protons.erase(std::unique(result.protons.begin(), result.protons.end(), [](const auto& a, const auto& b) {
        return a.directory == b.directory;
    }), result.protons.end());
    return result;
}

SteamDiscovery discover_steam() {
    std::vector<fs::path> roots;
    const char* home = std::getenv("HOME");
    if (home && fs::path(home).is_absolute()) {
        const fs::path h(home);
        roots = {h / ".local/share/Steam", h / ".steam/root", h / ".steam/steam",
                 h / ".var/app/com.valvesoftware.Steam/.local/share/Steam"};
    }
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    if (dataHome && fs::path(dataHome).is_absolute())
        roots.push_back(fs::path(dataHome) / "Steam");
    return discover_steam_in(roots);
}

std::string steam_discovery_text(const SteamDiscovery& d) {
    std::string out = "AstraPS5 Steam + Proton discovery (Linux PC; read-only)\n";
    out += "  Steam directory found: " + std::string(d.steam_directory_found ? "yes" : "no") + "\n";
    out += "  Libraries: " + std::to_string(d.libraries.size()) + "\n";
    out += "  Installed Steam entries: " + std::to_string(d.games.size()) + "\n";
    for (const auto& game : d.games) out += "  - " + game.name + " [" + game.app_id + "]\n";
    out += "  Proton installations detected: " + std::to_string(d.protons.size()) + "\n";
    for (const auto& proton : d.protons) out += "  - " + proton.name + (proton.custom ? " (custom)" : " (Steam)") + "\n";
    out += "  Steam launch/Proton compatibility: NOT TESTED BY ASTRA\n";
    out += "  PS5 hardware: NOT TESTED\n";
    return out;
}

std::string steam_discovery_json(const SteamDiscovery& d) {
    std::string out = "{\n  \"platform\": \"linux-pc\",\n  \"ps5_tested\": false,\n";
    out += "  \"steam_found\": " + std::string(d.steam_directory_found ? "true" : "false") + ",\n";
    out += "  \"library_count\": " + std::to_string(d.libraries.size()) + ",\n";
    out += "  \"game_count\": " + std::to_string(d.games.size()) + ",\n";
    out += "  \"proton_count\": " + std::to_string(d.protons.size()) + ",\n";
    out += "  \"games\": [\n";
    for (std::size_t i = 0; i < d.games.size(); ++i) {
        const auto& game = d.games[i];
        out += "    {\"app_id\": \"" + escape_json(game.app_id) + "\", \"name\": \"" +
               escape_json(game.name) + "\", \"install_path\": \"" +
               escape_json(game.installed_path.string()) + "\"}";
        out += i + 1 == d.games.size() ? "\n" : ",\n";
    }
    out += "  ],\n  \"proton\": [\n";
    for (std::size_t i = 0; i < d.protons.size(); ++i) {
        const auto& tool = d.protons[i];
        out += "    {\"name\": \"" + escape_json(tool.name) + "\", \"path\": \"" +
               escape_json(tool.directory.string()) + "\", \"custom\": " +
               (tool.custom ? "true" : "false") + "}";
        out += i + 1 == d.protons.size() ? "\n" : ",\n";
    }
    out += "  ]\n}\n";
    return out;
}

} // namespace astra
