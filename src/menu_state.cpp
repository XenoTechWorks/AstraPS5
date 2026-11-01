// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/menu_state.hpp"
#include <algorithm>

namespace astra {
void menu_move(MenuState& state, int direction) {
    if (direction == 0) return;
    if (state.screen == MenuScreen::Diagnostics) {
        state.scroll_lines = std::max(0, state.scroll_lines + direction);
        return;
    }
    state.selected = state.selected == MenuAction::Diagnostics
        ? MenuAction::Exit : MenuAction::Diagnostics;
}
void menu_select(MenuState& state) {
    if (state.screen != MenuScreen::Home) return;
    if (state.selected == MenuAction::Exit) {
        state.running = false;
    } else {
        state.scroll_lines = 0;
        state.screen = MenuScreen::Diagnostics;
    }
}
void menu_back(MenuState& state) {
    if (state.screen == MenuScreen::Diagnostics) {
        state.screen = MenuScreen::Home;
        state.scroll_lines = 0;
    } else {
        state.running = false;
    }
}
} // namespace astra
