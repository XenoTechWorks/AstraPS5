// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/menu_state.hpp"
#include <algorithm>

namespace astra {
void menu_move(MenuState& state, int direction) {
    if (direction == 0) return;
    const int step = direction > 0 ? 1 : -1;
    if (state.screen == MenuScreen::Diagnostics) {
        state.scroll_lines = std::max(0, state.scroll_lines + direction);
    } else if (state.screen == MenuScreen::Games) {
        if (state.game_count == 0) return;
        const std::size_t count = state.game_count;
        // Clamp after game library refresh/uninstall.
        if (state.selected_game >= count) state.selected_game = count - 1;
        state.selected_game = (state.selected_game + count + step) % count;
    } else {
        const int index = static_cast<int>(state.selected);
        state.selected = static_cast<MenuAction>((index + step + 3) % 3);
    }
}

void menu_select(MenuState& state) {
    if (state.screen == MenuScreen::Games) {
        if (state.game_count > 0 && state.selected_game < state.game_count)
            state.launch_requested = true;
        return;
    }
    if (state.screen != MenuScreen::Home) return;
    switch (state.selected) {
        case MenuAction::Games:
            state.screen = MenuScreen::Games;
            break;
        case MenuAction::Diagnostics:
            state.scroll_lines = 0;
            state.screen = MenuScreen::Diagnostics;
            break;
        case MenuAction::Exit:
            state.running = false;
            break;
    }
}

void menu_back(MenuState& state) {
    if (state.screen != MenuScreen::Home) {
        state.screen = MenuScreen::Home;
        state.scroll_lines = 0;
        state.launch_requested = false;
    } else {
        state.running = false;
    }
}
} // namespace astra
