# Verification — 2026-11-11

Built on target Arch/Omarchy system with Qt 6.12 and GCC 16.2.

Automated Qt test run: 13 passed, 0 failed. Covered private endpoint validation,
ADB states, persisted phone metadata and both transports, endpoint/Android version changes,
wrong-device rejection, normal closure, crash restart, manual stop, five-attempt limit,
USB restoration of modern wireless debugging, theme/language handling, and real QML loading.
Fake device commands run with isolated settings and state paths.

The launcher desktop file passed desktop-file-validate. The package contents were inspected.
English and Danish panels were rendered offscreen, including a 480-pixel-wide Settings panel.
The CLI status and isolated skill-install commands were checked.
A transient user-service run verified the real authorized phone over USB and wireless,
stored both transport records, registered with the Omarchy tray, and displayed a tiled panel.
Historical notes and earlier endpoint references were imported locally with provenance.
Those private records are not part of the source repository.

Package SHA-256: `8ff8a237f795a3d43250d4fef972db424082d37c6f963e8c2b823c0c02a814b7`

Per-user installation completed without root authentication after the secure sudo cards expired.
The installed binary matches the tested build. The user launcher and unit validate; the unit is
active and enabled under graphical-session.target. The tray is registered with the active icon,
and the installed app is mirroring the verified phone over Wi-Fi. Historical records survived
multiple service restarts. A supervised failure test restarted the tray process successfully.
A fresh desktop login has not been performed. The system-wide Arch package is built but not
installed in pacman; the active installation is under the user's ~/.local/ directories.
No commits or pushes were made.


## 0.1.1 compact panel — 2026-11-11

Installed the updated per-user build. All 14 tests passed, including live QML checks for
three/two/one status columns at different widths, actions staying near the top, sidebar
mouse navigation, Ctrl+B collapse, and independent startup preferences. English and
Danish layouts were inspected; the default panel is now 650×320 with a 44-pixel icon rail.
The expanded sidebar is 164 pixels. Status tiles are 108 pixels high and show full details
in tooltips. Actions remain outside the scrolling status area.

The main executable was checked offscreen with --tray: showPanel=true makes the panel
visible; showPanel=false leaves it hidden. Phone mirroring is controlled separately by
autoOpen. Existing preferences were preserved. The installed service remains enabled,
and the actual phone is mirroring over the verified wireless transport with no error.
The 0.1.1 Arch package is saved alongside the source; the active installation remains per-user.
No fresh desktop login was performed, and no commits or pushes were made.


## Release numbering

Official Git version tags begin at v0.0.0 for the initial commit. The earlier 0.1.x
numbers above identify pre-commit development builds; those historical verification
records remain unchanged. Source application and package metadata now use 0.0.0.
