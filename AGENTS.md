# Phonelink Android: agent instructions

## Scope and workspace

Read README.md before changing installation or connection behavior. This is an Omarchy
Android screen-mirroring / screen-copy tray manager around scrcpy, with USB debugging
and modern wireless debugging. It is tested on Omarchy / Hyprland; broader Hyprland
compatibility is expected only with the documented dependencies and is not yet verified.
Do not describe it as universally compatible with every Hyprland setup.

For this maintainer's workspace, the source of truth is this repository on Mirror at
`~/code/omarchy/phonelink`. Use SSH for source changes. Transfer a source snapshot to
the target Omarchy PC for builds, tests and installation. Mirror currently runs Mint
and has no Qt toolchain. Mirror is a development-workspace choice, not a runtime dependency.
Other contributors can use an ordinary source checkout on their target Linux desktop.
Do not commit, tag or push unless the user asks. Never move an existing published tag.

## Build and implementation

Use C++17, Qt Quick and qmake6. Qt 6.7 or newer is required by StateLocation. Keep ADB
and scrcpy operations asynchronous, bounded and outside QML layout code. The runtime
needs a session D-Bus and a StatusNotifierItem tray; supervised login startup also
needs systemd user services integrated with the graphical session.
Run `bin/build` and `bin/test` on the target toolchain after backend changes.
Use the real QML offscreen to verify responsive layout or interaction changes.
Use fake devices for failure tests. Verify actual phone/tray integration before claiming
it works on a new environment. Follow the Omarchy app skill when it is available for
theme, language, packaging and visual QA. Do not modify packaged Omarchy files.

## Installation and Omarchy application launcher

Read the installer before running it. Do not use sudo to run makepkg itself.
On Omarchy, dependency installation is documented in README.md using `omarchy pkg add`.
Build and test with `./bin/build` and `./bin/test`.

Use `./bin/install-user --no-build` for a built per-user install without root access.
It installs the binary under `~/.local/lib/phonelink/`, terminal commands under
`~/.local/bin/`, the desktop entry under the XDG data applications directory, SVG
phone icons under its hicolor icon tree, and the unit under the XDG config systemd/user
directory. Default paths are listed in README.md. Ensure the user's command directory
is on PATH. The installer reloads systemd and enables/starts phonelink.service.

Use `./bin/install` for an Arch package managed by pacman. It runs makepkg -fsi and
installs the same launcher/icon/unit integration at system locations. For an existing
per-user install, first stop it and deliberately remove only its installation files
before changing methods. A user unit or desktop entry can shadow a system installation.
Never remove settings, history or ADB keys during that switch.

The desktop entry must keep the name **Phonelink (scrcpy)**, its scrcpy/Android/phone
keywords, StartupWMClass=phonelink, and a valid phonelink-launch Exec. The system
package uses Icon=phonelink; the per-user installer resolves Icon to its installed SVG
path so a long-running launcher does not depend on a stale icon-theme cache.
The launcher starts the user service before showing its panel. Installing this desktop
entry and its icon already makes the app available in Omarchy's Super+Space application
launcher; typing scr or scrcpy should match it. Do not add an Omarchy menu extension or
Hyprland window rule merely to make it appear there. The tray icon may be in the drawer.

After installation, validate the installed desktop entry with desktop-file-validate.
For a stale per-user launcher, run `update-desktop-database ~/.local/share/applications`
(or the corresponding XDG data path), then reopen the application launcher.
Verify `systemctl --user is-enabled phonelink.service`, `is-active phonelink.service`,
and `is-active graphical-session.target`; use `systemctl --user cat phonelink.service`
to identify an unintended user override. Inspect the unit's journal if startup fails.
Do not reset/restart the whole Omarchy desktop for a launcher metadata change.

On other Hyprland desktops, confirm the tray host, launcher support, session D-Bus,
Wayland environment and graphical-session.target activation. Use UWSM or equivalent
session integration; enabling the service alone does not provide it. Read the current
official Hyprland documentation before proposing session configuration edits.
Without Omarchy files, theme and language fall back; core mirroring should not need them.
Report untested compatibility as untested. Do not add distro-specific install claims
without checking the target's Qt version and tool locations.

## Updates, versions and data

Stop phonelink.service before replacing its running per-user binary. Check the Git
working tree before pulling. Build/test on the target, run the selected installer,
then reopen the panel and restore the previous screen state if the user was mirroring.
Verify the installed version, tray registration, service status and actual transport.
Do not interrupt an active phone session for documentation-only edits or a Git tag.

Keep annotated version tags in the `v0.0.1` form. Official versioning began at v0.0.0;
earlier 0.1.x notes describe pre-commit development builds. When a release bump is
requested, align src/main.cpp's application/CLI version and pkgbuild/PKGBUILD's pkgver.
Documentation-only releases do not require restarting the installed desktop app.

Preserve all phone history, prior endpoints and user configuration. Do not put live
phone identifiers, network addresses, private keys or pairing codes in the repository.
Do not reset ADB keys or enable legacy adb tcpip 5555. Verify the configured phone serial
before mirroring. Pairing approvals remain on the phone; use a secure secret tool if
an assistant needs a pairing code. Closing the panel hides it; normal scrcpy closure
must not trigger crash recovery. The tray's start-at-login behavior is independent of
showing the status panel and starting phone mirroring; both window options default off.


## Native Omarchy plugin

The root manifest declares io.github.lightcc.phonelink, with BarWidget.qml as its
barWidget entry point. This is separate from the standalone src/Main.qml and from
the systemd backend. Do not load ApplicationWindow or C++ context objects inside the
shell. Use the existing service through plugin/bridge.py; never create a second
Quickshell process or parallel ADB monitor. Native plugin changes should pass
bin/test-plugin; backend/tray changes should also pass bin/test.

Left-click opens the existing tiled panel when ready, or native guided setup when the
backend is unavailable. Right-click opens setup, middle-click refreshes. Setup uses
Ui.Panel/KeyboardPanel, dismisses with Escape/outside click, and exposes the
phonelinkSetup open/close/toggle IPC target. Phone-debugging setup remains issue 1.
Backend 0.0.2 advertises pluginIntegration in its state contract.

plugin/setup.py owns guided per-user service installation, not ADB monitoring. Explicit
confirmation is required before a detached, flock-serialized worker copies source to
XDG_CACHE_HOME/phonelink/service-setup and builds. Keep the current service alive until
the build succeeds; back up owned files and prior lifecycle before stopping it. Use
bin/install-user --no-build --no-service, apply the confirmed login choice, verify IPC,
and roll back failures/cancellation. Preserve config, history and ADB keys. Never
silently replace system-managed/custom installs, symlinked files or unsupervised apps.
A compatible stopped backend should start without requiring build tools. No sudo or
package manager is invoked by setup; copyable agent instructions are user-mediated.
A killed worker may leave an interrupted transaction: inspect backups and recorded
paths/state before recovery, and never clear that guard merely to permit another install.
Use isolated HOME/XDG paths and mock services for failure tests. Test cancellation,
concurrency, rollback, source staging and live IPC on the target. Setup logs and job
state must remain bounded and private, outside the plugin directory, so shell file
watchers are not triggered by installer progress. The installed unit/binary must not
depend on the plugin clone remaining present.

The bridge forwards only a fixed boolean status summary. Keep subprocess calls
bounded, use argument arrays and retain the private-data/markup regression test.
A short lease at XDG_RUNTIME_DIR/phonelink-plugin-active suppresses the standalone
tray only while widget refreshes continue. Never persistently disable that tray
merely because the native plugin was installed. On disable/remove or shell failure,
the standalone service and its data must remain accessible after lease expiry.
Do not package the developer's build artifacts or personal preview screenshots.
