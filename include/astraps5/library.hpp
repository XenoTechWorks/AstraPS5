// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "astraps5/game.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace astra {

// A read-only snapshot of installed Flatpak *games* on the current Linux PC.
// Sources: user/system Flatpak-exported .desktop files categorized as Game.
// This is not a Steam library, PS5 compatibility database, or game installer.
struct GameLibrary {
    std::vector<GameProfile> games;
    std::string error; // Empty library is not an error when Flatpak is available.
};

// Discovers standard Flatpak desktop export directories, then inspects only
// valid Game-category entries and verifies each app is installed via Flatpak.
GameLibrary discover_flatpak_games();

// Explicit export directories for isolated tests and future platform adapters.
// Paths refer to the .../exports/share/applications folder, not generic desktop
// application directories. No desktop Exec= value is ever executed.
GameLibrary discover_flatpak_games_in(const std::vector<std::filesystem::path>& export_dirs);

std::string game_library_text(const GameLibrary& library);
std::string game_library_json(const GameLibrary& library);

} // namespace astra
