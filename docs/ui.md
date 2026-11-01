# Minimal native Astra menu (v0.2.0-dev)

**Status:** Real native Linux executable UI prototype. **Not a PS5 app, PS5 boot system, or game launcher.**

This intentionally sparse menu exists to connect UI work to the existing C++ codebase, rather than creating a standalone HTML or JS demo.

## Behavior

- `astra menu` opens a small native X11 window. An XWayland server can provide X11 compatibility in a Wayland session.
- Home screen offers just **Diagnostics** and **Exit**.
- Diagnostics calls `astra::collect_diagnostics()` from the same `astra_diagnostics` library used by `astra doctor`, and displays its real text report.
- Keyboard: Up/Down, Enter, Escape; mouse: hover/click; scroll diagnostics with arrows or wheel.
- This menu does not need PS5 hardware and does not claim to recognize it. It needs no elevated permissions.
- No game launching, fake tiles, or future-only actions are exposed.

## Integration boundary

The menu is a small presentation layer, not a bootloader:

```text
src/main.cpp  (astra menu)
     ├── astra::run_menu()       src/menu_x11.cpp
     │       ├── menu state      src/menu_state.cpp
     │       └── diagnostics     src/diagnostics.cpp
     └── other commands         src/diagnostics.cpp
```

Future PS5-side entry programs may need different APIs and build systems. The real PS5 entry app and boot code will be separate modules; **do not try to compile this X11 UI as a native PS5 home-screen executable**.

## Design choice

Xlib was selected for a dependency-light *Linux prototype* that can be built and tested now. For a production Astra Shell, a modern renderer supporting Wayland, controller focus, resolutions, and graphical acceleration should replace the X11 implementation without rewriting the diagnostics/core logic.

## Limitations

- PC keyboard and mouse only; no controller input yet.
- This UI requires an X11/XWayland display and does not work directly on the PlayStation system UI.
- The current hardware checks are best-effort diagnostic probes, not proof that PS5 drivers or game runtimes are ready.
