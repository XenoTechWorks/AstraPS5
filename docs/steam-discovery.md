# Steam and Proton discovery — v0.2.5-dev

> **Experimental Linux-PC-only feature.** This is a **read-only metadata scanner**, not Steam execution, Proton compatibility, PS5 support, Steam sign-in, or official online multiplayer.

## Why this exists

Until now AstraPS5 could launch native Linux and installed Flatpak games, while the game library menu displayed only Flatpaks. The new scanner reads local Steam installation data and displays installed Steam entries alongside Flatpak games. Steam entries are **display-only** and cannot be launched from Astra yet.

## Detection

`astra steam list` reads local Steam library data without contacting Steam servers or invoking the Steam client. The scanner supports these standard roots:

- `~/.local/share/Steam`
- `~/.steam/root` and `~/.steam/steam`
- `$XDG_DATA_HOME/Steam`
- `~/.var/app/com.valvesoftware.Steam/.local/share/Steam` (Flatpak Steam)

It reads `steamapps/libraryfolders.vdf` and `config/libraryfolders.vdf` (KeyValues text) to find extra library directories. In each `steamapps`, it examines `appmanifest_<appid>.acf` files, accepts a matching numeric app ID, requires a **Fully Installed** state bit, and verifies that the named directory exists inside `steamapps/common`.

Entries known to represent Steam's own runtimes and Proton are excluded from the game list. **Steam manifests can also describe non-game software**; Astra does not query Steam's catalog and cannot guarantee that every entry it lists is a game.

For Proton, Astra looks for local `proton` scripts under Steam library `steamapps/common/Proton...` directories. Custom builds are recognized when a subdirectory of `compatibilitytools.d` contains both `proton` and `compatibilitytool.vdf`. The scanner does not execute these scripts or attempt to identify which compatibility tool any particular game uses.

An installed Proton directory is **not** proof of working DirectX, Vulkan, Wine, game compatibility, or online support.

## Commands

```bash
./build/astra steam list
./build/astra steam list --json
./build/astra menu
```

On the **Games** screen, Flatpaks are labeled `Flatpak` and can be launched using the existing Astra launcher. Steam entries are marked `Steam / <appid> (discovery only)`; pressing Enter displays a short explanation. Press **R** to rescan local installations.

The pre-existing Flatpak-specific commands are unchanged:

```bash
./build/astra games list [--json]
./build/astra games run net.supertuxkart.SuperTuxKart
```

We intentionally do not invoke `steam -applaunch` yet: the Steam client may hand off to another process, so the status of the launcher process would not reliably indicate whether gameplay started or exited. Reliable Steam launch supervision and user-facing compatibility status belong in a later version.

## Safeguards and limits

- Metadata files, file size, parser depth, number of entries, and discovered libraries are bounded.
- No VDF-supplied shell command or arbitrary desktop-file `Exec=` is executed.
- Nonexistent, stale, malformed or partially downloaded app manifests are ignored.
- Only text KeyValues1 is supported; binary Steam cache files and non-Steam shortcuts are not parsed.
- No network or Steam account authentication occurs.
- External library directories found in Steam metadata are read-only; their files are not modified.
- The scanner relies on local metadata and may miss installs stored in unusual locations.
- Playability and anti-cheat compatibility are **not inferred** from installation metadata.

## Verified in development

- Automated C++ fixture tests cover libraryfolders.vdf (current and older layouts), local and external libraries, missing game directories, malformed manifests, ID mismatches, stale/incomplete installs, and bundled/custom Proton folders.
- GUI interaction was smoke-tested on Linux using simulated Steam and Flatpak installations in a virtual X11 display.
- This feature has **not** been tested with the user's actual Steam installation or on PS5 hardware.

References: [Valve's Proton documentation](https://github.com/ValveSoftware/Proton) and [Ubuntu's Steam Play guidance](https://ubuntu.com/docs/steam/howto/configure-proton/).
