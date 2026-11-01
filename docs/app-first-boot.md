# AstraPS5 app-first boot specification

> **Status: concept/design only.** A small Linux UI prototype exists, but no installable PS5 app or functional boot
> transition has been developed or verified by AstraPS5.

## Intended user experience

1. On a PS5 with a supported homebrew setup, the user sees **AstraPS5** in the
   console's normal home menu.
2. Selecting it shows a small confirmation and compatibility screen; not a Linux
   terminal. It confirms whether the Astra gaming image and relevant boot
   prerequisites are present.
3. Selecting **Launch AstraPS5** starts the supported Linux loader handoff.
   The first prototype may require entering rest mode and resuming the console,
   as documented by upstream PS5 Linux Loader.
4. The system starts the **Astra Shell** directly, instead of dropping users
   into GNOME/KDE or a shell. The PS5 controller navigates the gaming interface.
5. The player launches native Linux/Windows PC titles via an appropriate
   compatibility runtime. Supported titles may use their official PC services.
6. Returning to native PS5 OS is via a documented, safe system reboot initially.

## Supported versus aspirational scenarios

| Scenario | Status / expectation |
| --- | --- |
| Astra launched as a tile on supported, homebrew-capable PS5 | Proposed first boot target |
| Astra booted via existing PS5 Linux loader with external image | Integration target, not implemented |
| Launch on a fully updated, unmodified retail PS5 | **Not supported**; no known permitted mechanism |
| One button with no rest-mode handoff on every supported firmware | Future research; **not guaranteed** |
| Astra gaming Shell instead of a generic desktop | Minimal native Linux UI prototype only; real auto-start shell planned |
| PS5 game / PSN multiplayer support | Out of scope; targets PC game services |

## Initial integration contract (proposed)

Before touching actual boot control, design a versioned machine-readable
manifest containing:

- image version and cryptographic hash
- supported console model and firmware range (never wildcards by default)
- required loader version / interface
- root filesystem location and format
- kernel and initrd identification
- last-known-good configuration and recovery instructions

The Entry application must check availability and explain unsupported states;
it must not silently assume a working boot path.

## Development sequence

- [ ] Research supported PS5 SDK/homebrew tile packaging options for the test console.
- [ ] Choose the exact external loader integration point and its licensing obligations.
- [ ] Implement a read-only boot readiness check and versioned manifest parser
      with tests on ordinary Linux before running any PS5-side payload.
- [ ] Build a minimal Astra Entry proof of concept that shows launch readiness.
- [ ] Test initiating an **existing** trusted Linux boot payload on hardware.
- [ ] Package a minimal Astra System image with direct controller-first shell boot.
- [ ] Add an explicit recovery path and test error cases (missing USB/image,
      unsupported firmware, hash mismatch, failed resume).
- [ ] Validate a native Linux title, then Wine/Proton PC title, then an
      eligible official-PC-multiplayer title.

## First definition of success

On a compatible **real PS5**, launching the AstraPS5 icon from its home menu
hands off to a minimal Astra image and displays an Astra-branded screen
*without manually opening a desktop environment*. A normal reboot returns to
PS5 OS, and incorrect/missing images are rejected safely.

## Constraints

Do not assert this works on stock PS5 hardware or bypasses security restrictions.
This is an integration target for consoles that can already execute supported
homebrew/loader workflows. The project does not bundle proprietary code, games,
PSN credentials, or platform keys.
