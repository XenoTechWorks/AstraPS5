# Astra Shell — console-style C# UI (Linux PC)

Astra Shell is an experimental, separate **Avalonia 11 / .NET 8** UI for AstraPS5.
It takes PS4-inspired horizontal home navigation, Xbox-style quick-access tiles,
and Steam-inspired library search/filtering as high-level interaction ideas, with
an independent Astra color palette and layout.

**This is not a PlayStation 5 application and does not boot a PS5.** The old
`./build/astra menu` C++/X11 prototype remains functional and is not replaced.

## Build and run

First build the existing Astra Core (`./build/astra`) using CMake as documented
in the repository README. Install the .NET 8 SDK, then from the repository root:

```sh
dotnet restore shell/Astra.Shell/Astra.Shell.csproj
dotnet run --project shell/Astra.Shell/Astra.Shell.csproj
```

If Core is not in the normal `build/astra` location, specify its **absolute** path:

```sh
ASTRA_BIN=/absolute/path/to/astra dotnet run --project shell/Astra.Shell/Astra.Shell.csproj
```

The initial UI runs under X11 or XWayland on Linux. On a PC it reads actual
installed Flatpak games and locally installed Steam entries through the C++ CLI:

- `astra games list --json` — installed Flatpak games
- `astra steam list --json` — Steam installed titles, **read-only**
- `astra games run <app_id>` — Flatpak game launch; the UI stays responsive
- `astra doctor` — real host diagnostics

Astra does not download or infer game artwork, invent sample games, execute
unverified desktop `Exec` fields, or try to launch Steam games. Steam titles
show **Discovery only** and their Play controls are disabled. If the backend
is missing or discovery fails, the UI shows an actionable error and an empty
library instead of fictitious cards. The interface is keyboard/mouse first; real controller navigation is still planned.

## Astra Settings (v0.8)

Select **Settings** in the top navigation. Working, locally persisted
preferences include:

- **Game tile size**: Compact, Standard, Large. Home and Library tiles also
  respond to the window width while keeping portrait cover proportions.
- **Animated wallpaper**: pause/restart Astra's background motion.
- **Use game artwork as the background**: show locally discovered hero art for
  the selected game, or always use Astra's animated background instead.
- **Show seconds on the clock**: can also be toggled by double-clicking the clock.

### v0.8 visual balance

The home screen uses a compact **selected-game spotlight** with left-aligned
title and one main action: **Play now** for launchable games, or
**View game** for titles Astra Core cannot launch yet. Below it, a PS4-inspired horizontal
rail shows portrait game covers. Tiles respond to the window width and the
selected card grows slightly with clear focus borders. Common utility launchers
remain visible in Library but no longer crowd the home games rail.

When a real game cover is not available locally, Astra shows a stable,
title-based typographic placeholder with a distinct muted palette. It does not
invent cover art or stretch a desktop app icon. Existing artwork still takes
precedence and game hero backgrounds still crossfade, with stronger contrast
to keep titles readable. The top bar and settings area stay unchanged.

A temporary toast replaces the old persistent status footer for launches and
errors. Game art is cached for the current session.

Preferences are stored in the user's standard application config directory
(`~/.config/astra/shell-settings.json` on a typical Linux setup). Settings
also provides links to **What's New**, **Diagnostics**, and **Controls & help**.

Game tiles display a two-line title overlay whether or not artwork is found.
They never substitute a Flatpak app icon for cover artwork. Selecting a game
opens its details, which provides game-specific settings. Proton switching
and custom launch arguments in that dialog are deliberately disabled until
Astra Core supports them.


## Key mappings

- Home: Left/Right changes selected game, Enter selects a focused tile, Escape returns home.
- Library: text search filters by name; Source cycles All → Flatpak → Steam.
- Game details: available Flatpak titles expose Play; Steam entries are informational.
- Home: select a cover to open its game details; browse the full Library or open Settings from the header.
- Settings: appearance preferences, release notes, diagnostics, and welcome help.

## First boot-to-shell experiment (ordinary Linux only)

Build the UI with `dotnet publish shell/Astra.Shell -c Release`, then run:

```sh
sh scripts/astra-session.sh
```

The supervisor checks for actual Core/Shell binaries and retries a crashing
Shell at most three times before returning to its caller. It makes **no storage
changes** and does not install a boot service or change the host login manager.
This is not a PS5 handoff, PS5 firmware readiness check, Linux ISO, or new bootloader.

## Development contracts

Keep Astra Core in C++ with its existing CLI behavior. Avalonia UI can later
use a long-lived local IPC service, after a stable protocol, process supervision,
and integration tests are available. Today only the CLI is invoked via safe,
non-shell `ProcessStartInfo.ArgumentList`, preserving process and UI isolation.

The parser smoke tests run with:

```sh
dotnet run --project shell/Astra.Shell.Smoke/Astra.Shell.Smoke.csproj
```

Next milestones: controller input handling, persistent pinned collections,
a dedicated Linux graphical session,
and separately researched **PS5 loader integration only on compatible hardware**.
