# First PS5 hardware test plan

**AstraPS5 is experimental. This plan assumes you already have a supported,
working PS5 Linux installation.** It does not tell you how to exploit or
modify PS5 firmware or bootloaders.

1. Boot your established PS5 Linux environment and verify graphics work normally.
2. Open a terminal in that environment. Install development tools using the
   distribution's *normal supported method* (CMake, a C++17 compiler, and
   optionally `vulkaninfo` from `vulkan-tools`).
3. Clone/download the AstraPS5 source repository and run:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j2
   ctest --test-dir build --output-on-failure
   ./build/astra doctor
   ./build/astra doctor --json > astra-report.json
   ```

4. Read the results **locally** first. A PS5 marker is not trusted proof of
   hardware identity; Vulkan enumeration is not proof a game will work.
5. Before sharing a report, review it for potentially identifying local paths,
   GPU and system information. Never post credentials or serial numbers.
6. Compare graphics output with `vulkaninfo --summary` and the current PS5
   Linux documentation; report distro, kernel, Mesa version, Vulkan GPU name,
   and controller state only if you choose to.

## Pass/fail criteria

- Build and 4 tests pass on the real PS5 Linux environment.
- `astra doctor --json` is valid JSON and includes all checks.
- `vulkaninfo` returns a named Vulkan device (if graphics is functional).
- Steam/Proton checks are informational. A missing result could mean a
  nonstandard install path rather than missing functionality.

**Next milestone:** successfully run an authorized *native Linux* open-source
rendering/game test on PS5 Linux with recorded FPS and audio/input results.
Do not start with an online-only anti-cheat-protected multiplayer game.
