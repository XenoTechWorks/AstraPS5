# Astra Flatpak game library — v0.2.5-dev

> **Experimental Linux-PC functionality.** This Flatpak discovery component does not check whether a game can run on PS5 hardware. Astra now has a **separate**, read-only Steam/Proton scanner described in [Steam and Proton discovery](steam-discovery.md). Neither component enables Steam or Proton game execution on PS5, multiplayer, or any console boot environment.

## What it does

AstraPS5 discovers *installed Flatpak applications categorized as games* and displays them alongside Steam entries in a minimal native C++ menu. No fake title cards or store integrations are involved.

The discovery process:

1. Checks standard per-user and system Flatpak desktop-export locations (as described in the [Flatpak conventions](https://docs.flatpak.org/en/latest/conventions.html#desktop-files)).
2. Reads exported `.desktop` metadata files, with a size limit and a cap on scanned files.
3. Requires `[Desktop Entry]`, `Type=Application`, `Categories=Game;` and a nonempty `Name`; hidden and non-display applications are skipped.
4. Uses the desktop export filename's app ID (`<app_id>.desktop`), checks its format, and verifies the application through `flatpak info --show-ref <app_id>`.
5. Sorts entries by name and discards duplicate IDs.

Only entries with the exact `<app_id>.desktop` export filename are currently supported. Some Flatpaks export secondary launchers using a suffixed name (for example `<app_id>.foo.desktop`), which this early scanner does not discover. Games without a `Game` desktop category are not listed. Custom Flatpak installations that don't export into these directories may not be found yet.

**Safety:** Astra never interprets `Exec=` from desktop files or invokes a shell for game launching. It only runs `flatpak run <validated_app_id>` via the shared C++ launcher. Apps still execute with their normal Flatpak permissions. Launch only software you trust.

## CLI

```bash
./build/astra games list
./build/astra games list --json
./build/astra games run net.supertuxkart.SuperTuxKart
```

The `games run` command refuses to run apps that aren't in Astra's discovered game list. For manually authored native/Flatpak game profiles, the existing `astra game check/run <profile>` commands remain supported.

## Minimal UI

```bash
./build/astra menu
```

On **Home**, select **Games**. On **Games**, use Up/Down to choose an installed game and Enter to launch it if it is a Flatpak. Steam entries are currently discovery-only. Press **R** to refresh the list after an installation/uninstallation, or Escape to go back.

The window remains responsive while the game runs. Launch output is saved to a private per-session log in `$XDG_STATE_HOME/astraps5/logs` or `~/.local/state/astraps5/logs`. Review logs for personal data before sharing them.

The menu currently supports X11/XWayland mouse and keyboard only. The future PS5 app and gamepad support are not implemented.

## Actual test status

- **Linux PC:** User reported a complete SuperTuxKart race through the v0.2.2 launcher. **Gameplay verified by the user**, no online testing.
- **v0.2.4:** Development tests verified discovery, filtering, deduplication, CLI JSON, process launching, and game selection using **mock Flatpak programs** on a Linux development environment.
- **PS5 hardware:** Not tested or verified.
- **Official multiplayer:** Not tested or implemented.
