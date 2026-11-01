// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/menu_state.hpp"
#include <cassert>
int main() {
    astra::MenuState state{};
    assert(state.running);
    assert(state.screen == astra::MenuScreen::Home);
    assert(state.selected == astra::MenuAction::Diagnostics);
    astra::menu_select(state);
    assert(state.screen == astra::MenuScreen::Diagnostics);
    astra::menu_move(state, 5);
    assert(state.scroll_lines == 5);
    astra::menu_move(state, -100);
    assert(state.scroll_lines == 0);
    astra::menu_back(state);
    assert(state.screen == astra::MenuScreen::Home);
    astra::menu_move(state, 1);
    assert(state.selected == astra::MenuAction::Exit);
    astra::menu_select(state);
    assert(!state.running);
    return 0;
}
