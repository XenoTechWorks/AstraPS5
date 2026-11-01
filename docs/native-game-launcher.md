# Astra Linux game launching — v0.2.5-dev

> **EXPERIMENTAL TEST PROJECT:** These features run existing applications on a normal Linux PC. Nothing here emulates a PS5, boots a console, verifies PS5 GPU support, or enables multiplayer.

## Flatpak — our first real game test

The user confirmed SuperTuxKart's Flatpak version launched through AstraPS5 and a race was playable on their Linux PC (November 1, 2026). Online multiplayer and PS5 hardware have **not** been tested.

If SuperTuxKart is already installed as a Flatpak, open **`astra menu` → Games** and choose SuperTuxKart. Astra scans installed Game-category Flatpaks rather than hardcoding a single app ID. No hardcoded executable paths are needed.

CLI alternative:

```bash
./build/astra game check examples/supertuxkart-flatpak.game.example
./build/astra game check examples/supertuxkart-flatpak.game.example --json
./build/astra game run examples/supertuxkart-flatpak.game.example
```

The game profile is intentionally small:

```ini
format=astra-game-v1
name=SuperTuxKart
runtime=flatpak
app_id=net.supertuxkart.SuperTuxKart
```

Astra verifies that the `flatpak` command exists and that `flatpak info --show-ref <app_id>` succeeds before launching with literal arguments using `flatpak run <app_id>`. These checks show whether the app is installed, not whether a game will actually be playable.

## Native Linux package

The existing v0.2.2 profile format is still supported:

```ini
format=astra-game-v1
name=SuperTuxKart
executable=supertuxkart
```

Set `runtime=native` optionally; omit it for backward compatibility. `executable` may be an absolute path, path relative to its profile, or executable found in PATH. A profile cannot mix `executable` and Flatpak `app_id`.

## Logs, exit codes and the UI

- Every launch captures the spawned process's stdout and stderr in a new private log under `$XDG_STATE_HOME/astraps5/logs/`, or `~/.local/state/astraps5/logs/` when `XDG_STATE_HOME` is unset.
- The CLI prints the log location and waits for the child, then reports its exit status. The UI polls the child without freezing the interface and reports the exit code when it finishes.
- Child exit status `0` means the launched process exited successfully, **not** that gameplay, graphics or online services worked.
- Flatpak may use an already-running application instance; process exit status alone cannot verify the game's lifetime or rendering.
- Logs may contain user paths, game tokens, or other sensitive output. Review and redact them before sharing publicly.
- The log directory must be writable and HOME/XDG_STATE_HOME must be an absolute path.

Astra uses POSIX `posix_spawn()` and `waitpid()`; **it does not execute profile values through a shell**. Arguments are plain literal values `arg0`...`arg15`, each passed as one argument. Game profiles can run arbitrary user-accessible programs: only launch profiles you trust.

## Current limitations

- The minimal window uses X11/XWayland and keyboard/mouse. No native PS5 tile, automatic console boot, or controller-driven Astra UI yet.
- The menu now discovers installed **Flatpak Game-category apps** through exported desktop metadata; see [dynamic game library](game-library.md). Native executables still require explicit game profiles. Steam manifest and Proton directory discovery is available separately via `astra steam list`, but Steam launching and Proton execution are not implemented.
- Logs and exit codes are informational; crash analysis, stack traces and performance monitoring are future work.
- Wine/Proton, PS5 hardware, GPU driver compatibility, online multiplayer and anti-cheat have not been implemented or verified.

## Quick build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/astra menu
```
