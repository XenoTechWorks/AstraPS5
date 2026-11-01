# AstraPS5 architecture (app-first proposal)

**EXPERIMENTAL TEST PROJECT.** The diagnostic CLI and minimal native Linux menu prototype exist today.
No PS5 home-menu tile, boot manager, custom distro, game launcher or playable
PC games are implemented by AstraPS5 at this stage.

## Product requirement

The user-visible entry point is an **AstraPS5 app/tile in the PS5 home screen**.
On a supported system, launching it should initiate a safe handoff to a
self-contained Astra gaming environment. The user should not need to boot a
separate Linux desktop and manually invoke Wine or Proton.

This is distinct from claiming unrestricted installation on stock PS5 firmware.
The PS5 OS application sandbox cannot independently boot arbitrary operating
systems. Initial target: supported homebrew-capable consoles and proven PS5
Linux bootloader interfaces, with explicit firmware/hardware compatibility.

## Proposed components and boundaries

1. **Astra Entry** (PS5 side, not implemented): tile/application that shows
   compatibility and explicit `Launch AstraPS5`/`Cancel` controls. Uses only
   supported homebrew APIs and available loader integration; no firmware code
   or console secrets shipped.
2. **Astra Boot** (PS5 side, not implemented): verifies a signed/checksummed
   image manifest, detects supported model/firmware/loader, checks storage and
   power requirements, and calls an externally supplied compatible loader.
   If validation fails, it returns an error without changing boot state.
3. **Astra System** (Linux boot image, not implemented): custom gaming-oriented
   root filesystem with the required kernel/driver/runtime stack. Initially
   stored separately from the PS5 internal system partition and booted using
   established community techniques.
4. **Astra Shell** (Linux, prototype only): a minimal X11 window currently shows
   real Astra diagnostics and Exit. The future auto-starting, controller-first
   gaming shell is not implemented. See [ui.md](ui.md).
5. **Astra Core** (Linux, implemented): C++17 diagnostics reporting readable
   text and JSON, useful both during development and later in the Shell.
6. **Astra Runtime / Profiles** (Linux, not implemented): game launch, Wine /
   Proton integration, logs, hardware-specific configuration, and game profiles.

## System flow

```text
PS5 system menu
   |
   v
[Astra Entry]  -- cancel / unsupported --> PS5 system menu
   |
   v
[Astra Boot preflight] -- failed --> PS5 system menu + actionable error
   |
   v
[Compatible PS5 Linux loader] -- boot handoff / rest mode if required
   |
   v
[Astra System: kernel / drivers / gaming image]
   |
   v
[Astra Shell: games / settings / diagnostics]
   |
   v
[PC game: native Linux or Wine/Proton]
```

Returning to PS5 OS may require a reboot. A seamless in-process transition
back to the native menu is **not** an initial promise.

## Existing upstream building blocks

- PS5 Linux Loader: https://github.com/ps5-linux/ps5-linux-loader
- PS5 Linux Manager: https://github.com/MounirHero/PS5-Linux-Manager
- Community PS5 Linux hardware guide: https://guide.rmux.eu/guide/what-is-this
- PS5 Bazzite Deck: https://github.com/VivaLaVent/ps5-bazzite-deck
- Wine / Proton / Vulkan / Mesa: used under their original licenses.

An external existing loader is a dependency candidate, **not** code we can
assume is Apache-2.0. Review GPL-3.0 compatibility and distribution obligations
before integration or bundling.

## Safety and support policy

- Don't modify Sony's system partitions in an initial prototype.
- Never misrepresent firmware compatibility; detect or explicitly specify it.
- Verify images, ensure safe failure modes, and provide boot recovery guidance.
- No proprietary firmware, games, console credentials, or security keys in repo.
- No spoofed PSN identity or anti-cheat circumvention.
- Never promise official multiplayer, even when a PC game successfully launches.
- Use independently verified PS5 hardware tests; desktop tests alone don't prove boot.

See [app-first-boot.md](app-first-boot.md) for milestones and acceptance criteria.
