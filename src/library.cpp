// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/library.hpp"
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
#include <vector>

namespace fs = std::filesystem;
namespace astra {
namespace {
constexpr std::uintmax_t kMaxDesktopBytes = 65536;
constexpr std::size_t kMaxCandidates = 256;
constexpr std::size_t kMaxGames = 80;

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

bool app_id_safe(const std::string& id) {
    if (id.size() < 5 || id.size() > 190 || id.front() == '.' || id.back() == '.' ||
        id.find("..") != std::string::npos || id.find('.') == std::string::npos) return false;
    for (unsigned char ch : id) {
        if (!std::isalnum(ch) && ch != '_' && ch != '-' && ch != '.') return false;
    }
    return true;
}

bool categories_have_game(const std::string& categories) {
    std::istringstream stream(categories);
    std::string category;
    while (std::getline(stream, category, ';')) {
        if (trim(category) == "Game") return true;
    }
    return false;
}

bool safe_name(const std::string& name) {
    if (name.empty() || name.size() > 96) return false;
    for (unsigned char ch : name) if (ch < 32 || ch == 127) return false;
    return true;
}

// A desktop file is metadata only: NEVER interpret its Exec= field.
// Limit bytes; reject hidden entries and non-games; read the base Name key.
bool desktop_game_name(const fs::path& file, std::string& name) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return false;
    const auto size = fs::file_size(file, ec);
    if (ec || size > kMaxDesktopBytes) return false;
    std::ifstream input(file);
    if (!input) return false;
    bool in_entry = false;
    std::string type, category, hidden, no_display;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            in_entry = (line == "[Desktop Entry]");
            continue;
        }
        if (!in_entry) continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        auto key = trim(line.substr(0, pos));
        auto value = trim(line.substr(pos + 1));
        if (key == "Name") name = value;
        else if (key == "Type") type = value;
        else if (key == "Categories") category = value;
        else if (key == "Hidden") hidden = value;
        else if (key == "NoDisplay") no_display = value;
    }
    return !input.bad() && type == "Application" && categories_have_game(category) &&
           hidden != "true" && no_display != "true" && safe_name(name);
}

// Only trusted Flatpak export locations are discovered automatically.
std::vector<fs::path> standard_export_dirs() {
    std::vector<fs::path> dirs;
    const char* xdg_home = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg_home && fs::path(xdg_home).is_absolute())
        dirs.emplace_back(fs::path(xdg_home) / "flatpak/exports/share/applications");
    else if (home && fs::path(home).is_absolute())
        dirs.emplace_back(fs::path(home) / ".local/share/flatpak/exports/share/applications");
    dirs.emplace_back("/var/lib/flatpak/exports/share/applications");
    if (const char* xdg_dirs = std::getenv("XDG_DATA_DIRS")) {
        std::istringstream stream(xdg_dirs);
        std::string dir;
        while (std::getline(stream, dir, ':')) {
            const fs::path path(dir);
            if (!path.is_absolute()) continue;
            // Don't scan arbitrary desktop entries outside Flatpak exports.
            if (path.filename() == "share" && path.parent_path().filename() == "exports" &&
                path.parent_path().parent_path().filename() == "flatpak")
                dirs.emplace_back(path / "applications");
        }
    }
    return dirs;
}

} // namespace

GameLibrary discover_flatpak_games_in(const std::vector<fs::path>& export_dirs) {
    GameLibrary result;
    std::map<std::string, std::string> candidates; // ID -> human-readable name
    std::set<fs::path> visited;
    std::size_t examined = 0;

    for (const auto& dir : export_dirs) {
        std::error_code ec;
        auto absolute = fs::absolute(dir, ec);
        if (ec || !visited.insert(absolute.lexically_normal()).second ||
            !fs::is_directory(dir, ec)) continue;
        fs::directory_iterator iter(dir, fs::directory_options::skip_permission_denied, ec);
        const fs::directory_iterator end;
        for (; !ec && iter != end && examined < kMaxCandidates; iter.increment(ec)) {
            const auto file = iter->path();
            if (file.extension() != ".desktop") continue;
            ++examined;
            const std::string id = file.stem().string();
            if (!app_id_safe(id) || candidates.count(id)) continue;
            std::string name;
            if (desktop_game_name(file, name)) candidates.emplace(id, name);
        }
        if (examined >= kMaxCandidates) break;
    }

    for (const auto& [id, name] : candidates) {
        if (result.games.size() >= kMaxGames) break;
        GameProfile game = inspect_flatpak_app(name, id);
        if (game.valid) result.games.push_back(std::move(game));
        // Invalid or uninstalled .desktop exports are deliberately ignored.
    }
    std::sort(result.games.begin(), result.games.end(), [](const GameProfile& a, const GameProfile& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.flatpak_id < b.flatpak_id;
    });
    return result;
}

GameLibrary discover_flatpak_games() {
    // Give users an actionable diagnostic if Flatpak is not installed.
    GameLibrary library = discover_flatpak_games_in(standard_export_dirs());
    // An empty library may simply mean the user hasn't installed any games.
    return library;
}

std::string game_library_text(const GameLibrary& library) {
    std::string result = "AstraPS5 local Flatpak game library (Linux PC)\n";
    result += "  Games: " + std::to_string(library.games.size()) + "\n";
    for (const auto& game : library.games)
        result += "  - " + game.name + " [" + game.flatpak_id + "]\n";
    if (!library.error.empty()) result += "  Note: " + library.error + "\n";
    result += "  PS5 compatibility: NOT TESTED\n";
    return result;
}

std::string game_library_json(const GameLibrary& library) {
    std::string out = "{\n  \"platform\": \"linux-pc\",\n  \"ps5_tested\": false,\n";
    out += "  \"count\": " + std::to_string(library.games.size()) + ",\n";
    out += "  \"error\": \"" + escape_json(library.error) + "\",\n  \"games\": [\n";
    for (std::size_t i = 0; i < library.games.size(); ++i) {
        const auto& game = library.games[i];
        out += "    {\"name\": \"" + escape_json(game.name) + "\", \"app_id\": \"" +
               escape_json(game.flatpak_id) + "\", \"runtime\": \"flatpak\"}";
        out += i + 1 == library.games.size() ? "\n" : ",\n";
    }
    out += "  ]\n}\n";
    return out;
}

} // namespace astra
