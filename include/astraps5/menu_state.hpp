// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>

namespace astra {

// Pure navigation state: UI drawing and Flatpak process execution are separate.
enum class MenuAction { Games, Diagnostics, Exit };
enum class MenuScreen { Home, Games, Diagnostics };

struct MenuState {
    MenuScreen screen = MenuScreen::Home;
    MenuAction selected = MenuAction::Games;
    std::size_t game_count = 0;
    std::size_t selected_game = 0;
    bool launch_requested = false;
    int scroll_lines = 0;
    bool running = true;
};

void menu_move(MenuState& state, int direction);
void menu_select(MenuState& state);
void menu_back(MenuState& state);
} // namespace astra
