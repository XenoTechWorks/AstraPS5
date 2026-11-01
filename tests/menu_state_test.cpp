// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/menu_state.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main() {
    astra::MenuState state{};
    assert(state.running && state.game_count == 0 && state.selected == astra::MenuAction::Games);
    astra::menu_select(state);
    assert(state.screen == astra::MenuScreen::Games);
    astra::menu_select(state);
    assert(!state.launch_requested); // an empty library cannot launch a game
    astra::menu_move(state, 1);
    assert(state.selected_game == 0);
    astra::menu_back(state);
    assert(state.screen == astra::MenuScreen::Home);
    astra::menu_move(state, 1);
    assert(state.selected == astra::MenuAction::Diagnostics);
    astra::menu_select(state);
    assert(state.screen == astra::MenuScreen::Diagnostics);
    astra::menu_move(state, 5);
    assert(state.scroll_lines == 5);
    astra::menu_move(state, -100);
    assert(state.scroll_lines == 0);
    astra::menu_back(state);
    astra::menu_move(state, 1);
    assert(state.selected == astra::MenuAction::Exit);
    astra::menu_select(state);
    assert(!state.running);

    astra::MenuState withGames{};
    withGames.game_count = 3;
    astra::menu_select(withGames);
    assert(withGames.screen == astra::MenuScreen::Games);
    astra::menu_move(withGames, -1);
    assert(withGames.selected_game == 2);
    astra::menu_move(withGames, 1);
    assert(withGames.selected_game == 0);
    astra::menu_select(withGames);
    assert(withGames.launch_requested);
    withGames.launch_requested = false;
    withGames.game_count = 1;
    withGames.selected_game = 2;
    astra::menu_select(withGames);
    assert(!withGames.launch_requested); // invalid selected game must not launch
    astra::menu_move(withGames, 1);
    assert(withGames.selected_game == 0); // selection recovers after refresh
    astra::menu_back(withGames);
    assert(withGames.screen == astra::MenuScreen::Home);
    assert(!withGames.launch_requested);
    return 0;
}
