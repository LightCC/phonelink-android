# Changelog

## 0.0.2 — Native plugin integration

Add the native Omarchy manifest, phone bar widget, bounded service bridge, translated
status messages and explicit backend prerequisites. The existing service remains
responsible for connections. A renewable runtime lease suppresses the duplicate
tray icon and restores standalone access after plugin removal. Guided service
installation is tracked separately in issue 5. Add bridge, lease and plugin checks.

## Unreleased

Fix #2: render a recognizable phone icon directly through the system-tray protocol,
including offline, connected and mirroring states. Per-user launcher icons resolve
to the installed SVG path. Document pinning the tray next to network indicators.

## v0.0.1 — 2026-11-11

Documentation release. Clarifies Omarchy / Android / scrcpy discovery and the distinction
between tested Omarchy support and expected, untested compatibility on other Hyprland
desktops. Adds first-phone setup, USB and wireless debugging, launcher integration,
login-service checks, installation methods, update steps and agent guidance.
Application and package version metadata is aligned to 0.0.1; runtime behavior is unchanged.

## v0.0.0 — 2026-11-11

Initial tagged source release: tray service, compact status panel, scrcpy control and
recovery, phone/endpoint history, user and Arch package installers, and launcher integration.
Earlier 0.1.x development-build numbers predate official Git release numbering.
