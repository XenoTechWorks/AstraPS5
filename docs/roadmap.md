# AstraPS5 roadmap

AstraPS5 is an experimental test project. All dates and compatibility outcomes remain open.

## M0 — Host diagnostics and minimal UI [prototype code]
- [x] Linux/architecture/kernel probe
- [x] PS5 Linux marker probe (not hardware verification)
- [x] Vulkan device report via `vulkaninfo --summary` when available
- [x] Steam / Wine / Proton availability checks
- [x] Input controller and network link visibility
- [x] JSON mode for future launchers and reporting
- [x] Minimal native C++/X11 menu using shared diagnostics library
- [x] Menu selection and keyboard navigation unit tests
- [x] Automated native Linux unit/CLI tests
- [ ] Run and archive real results on PS5 Linux hardware

## M1 — PS5 home-screen entry and safe boot handoff [future]
- [ ] Research tile/app packaging for supported homebrew-capable PS5 systems
- [ ] Establish external PS5 Linux loader integration and license boundaries
- [ ] Implement versioned image manifest validation and safe preflight checks
- [ ] Show an AstraPS5 app/tile with Launch and Cancel actions
- [ ] On real PS5 hardware, hand off to a minimal boot image
- [ ] Verify clean recovery on missing/bad images and safe reboot to PS5 OS

## M2 — Astra gaming system [future]
- [ ] Define reproducible AstraPS5 Linux gaming image build
- [ ] Replace desktop-X11 prototype with a controller-first, auto-start gaming shell
- [ ] Verify PS5-specific driver support, GPU acceleration and Vulkan
- [ ] Validate controller, audio, storage and network behavior

## M3 — Baseline gaming validation [future]
- [ ] Verify working Vulkan with a real rendering sample on supported PS5 Linux
- [ ] Establish baseline native Linux game test (authorized/free title)
- [ ] Validate input, audio, file I/O and frame timing
- [ ] Document GPU/kernel/Mesa/firmware combinations

## M4 — PC compatibility [future]
- [ ] Select Wine/Proton runtime without bundling copyrighted game files
- [ ] Profiles for per-game settings and safe environment variables
- [ ] Capture launch logs, exit status, and rendering symptoms
- [ ] Independently validate a Windows game on PS5 Linux

## M5 — Official PC multiplayer [future research]
- [ ] Test a game whose Linux/Proton online support is explicitly allowed
- [ ] Verify normal authentication against the game's official PC service
- [ ] Report anti-cheat/integration restrictions without evasion
- [ ] Add game-by-game verified compatibility evidence

## M6 — Usability [future]
- [ ] Controller-friendly Astra Launcher
- [ ] Public compatibility dashboard built from user-approved test reports
- [ ] Package the Astra System image and Astra Entry flow for supported PS5 hardware

## v0.2.1-dev — image integrity groundwork

- [x] Read-only boot manifest format v1, SHA-256 checksum verification, CLI/JSON output, negative tests.
- [ ] Console-specific entry app, boot handoff, game launch and PS5-specific hardware testing remain future work.
