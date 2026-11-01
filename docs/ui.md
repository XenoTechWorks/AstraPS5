# Minimal native Astra menu (v0.2.3-dev)

**Experimental Linux desktop prototype; not a PS5 app or boot system.**

The C++ X11/XWayland menu deliberately remains sparse. At start it probes for the installed **SuperTuxKart Flatpak**. When found, its home actions are:

1. **Launch SuperTuxKart** — executes the installed Flatpak through shared Astra Core; the window remains responsive while the game runs.
2. **Diagnostics** — displays the same real report as `astra doctor`.
3. **Exit** — closes Astra's menu.

Without that Flatpak, the menu shows only Diagnostics and Exit. No fake game library is shown. The launcher checks app presence again before each launch and reports completion and logs.

### Controls

Arrow Up / Arrow Down, Enter or Space, Escape, mouse hover/click. Pressing Escape from Diagnostics returns home. This UI does not yet support controllers and does not render game content itself.

### Architecture boundary

```text
src/main.cpp   astra menu
  └─ src/menu_x11.cpp         window and asynchronous process polling
      ├─ menu_state.cpp       state transitions
      ├─ diagnostics.cpp      host diagnostics
      └─ game.cpp             Flatpak presence, launch, log and exit status
```

Do not compile this desktop X11 prototype as a native PS5 app. A future Astra Entry / Astra Shell must target the actual supported boot environment and provide controller-first input.


## v0.2.4 — minimal installed-game list

The home screen now has Games, Diagnostics and Exit. Games lists *actual installed* Flatpak apps with a `Game` desktop category. Arrows select, Enter launches, R refreshes and Escape returns. The game UI is a plain text list, with no mock library cards or shop. It is still an X11 desktop development prototype—not a PS5 tile or console-shell app.
