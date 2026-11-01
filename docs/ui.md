# AstraPS5 native UI — v0.2.5-dev

The native C++/X11 menu remains deliberately bare-minimum. Its home actions are **Games**, **Diagnostics**, and **Exit**. No storefront, PS5 boot buttons, or placeholder game cards exist.

On the **Games** page:

- Installed Flatpak games appear with a **Flatpak** subtitle and use Astra's existing working game launcher.
- Local Steam entries appear with a **Steam / <appid> (discovery only)** subtitle. They cannot be launched yet. Selecting one explains the limitation.
- The header shows the number of detected Proton installations. This is **metadata detection**, not a compatibility result.
- Arrow keys move, Enter selects, R refreshes and Escape goes back. Mouse interaction is supported. A native gamepad UI and PS5 app are future work.

The **Diagnostics** page displays live Linux host information together with the Steam discovery summary. Everything is desktop-Linux only until tested on PS5 hardware.

For implementation details see [Steam and Proton discovery](steam-discovery.md) and [Flatpak game library](game-library.md).

## Historical changes

## v0.2.4 — minimal installed-game list

The home screen now has Games, Diagnostics and Exit. Games lists *actual installed* Flatpak apps with a `Game` desktop category. Arrows select, Enter launches, R refreshes and Escape returns. The game UI is a plain text list, with no mock library cards or shop. It is still an X11 desktop development prototype—not a PS5 tile or console-shell app.
