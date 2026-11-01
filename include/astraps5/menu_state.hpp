// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace astra {

// The menu intentionally contains only two real actions.
enum class MenuAction { Diagnostics, Exit };
enum class MenuScreen { Home, Diagnostics };

struct MenuState {
    MenuScreen screen = MenuScreen::Home;
    MenuAction selected = MenuAction::Diagnostics;
    int scroll_lines = 0;
    bool running = true;
};

void menu_move(MenuState& state, int direction);
void menu_select(MenuState& state);
void menu_back(MenuState& state);
} // namespace astra
