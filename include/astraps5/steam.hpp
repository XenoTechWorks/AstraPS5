// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace astra {

// Read-only metadata found on an ordinary Linux PC. None of this indicates
// PS5 compatibility, ownership verification, or Proton game compatibility.
struct SteamInstalledGame {
    std::string app_id;
    std::string name;
    std::filesystem::path installed_path;
};

struct ProtonInstall {
    std::string name;
    std::filesystem::path directory;
    bool custom = false;
};

struct SteamDiscovery {
    bool steam_directory_found = false;
    std::vector<std::filesystem::path> libraries;
    std::vector<SteamInstalledGame> games;
    std::vector<ProtonInstall> protons;
};

// Standard, local Steam directories (~/.local/share/Steam, ~/.steam/root,
// and XDG_DATA_HOME/Steam). This does not contact Valve or execute Steam.
SteamDiscovery discover_steam();

// Explicit root directories for deterministic tests. Each root must contain
// a steamapps directory. Nested libraries are read from libraryfolders.vdf.
SteamDiscovery discover_steam_in(const std::vector<std::filesystem::path>& steam_roots);

std::string steam_discovery_text(const SteamDiscovery& discovery);
std::string steam_discovery_json(const SteamDiscovery& discovery);

} // namespace astra
