# Phonelink

![Phonelink connection panel](docs/phonelink.png)

Phonelink keeps USB and wireless scrcpy connections visible from the Omarchy tray.
Click the phone icon to open a compact tiled status window. The icon sidebar expands with
its menu button or Ctrl+B. Connection actions stay above the scrolling content; the three
status tiles wrap into two or one columns as the window narrows. Close the panel to hide it;
monitoring continues in the user service. The phone screen is a separate scrcpy window.

## Connection behavior

The panel distinguishes physical USB presence, USB debugging authorization, wireless
ADB discovery, verified wireless connection, and the scrcpy process. Only the configured
phone serial is accepted. Automatic mode prefers verified wireless and can fall back to USB.
USB-only and wireless-only modes are available. Change modes while the screen is stopped.

Open Screen can restore modern wireless debugging through an authorized USB connection.
The explicit Restore Wireless via USB button does the same reset without opening the screen.
Wireless debugging is toggled off and on; no fixed port or legacy TCP listener is enabled.
ADB pairing keys are left intact. Android may still require approval or pairing on the phone.
Connection checks run every five seconds; failed endpoint connections wait twenty seconds
between attempts. An advertisement from the selected phone supplies a changed IP or port.

A normal screen closure stops mirroring. A failed scrcpy process gets up to five total
attempts, with delays of 1, 2, 4 and 8 seconds. A healthy minute resets the failure count.
Stop Screen cancels pending retries. The service itself uses systemd Restart=on-failure,
with at most five service starts per minute. Automatic phone mirroring and automatic status-panel opening are independent settings,
both off by default. The tray service always starts at graphical login.

## History

Open History Folder launches the file manager. History belongs to this PC, not the source repo.
The default folder is `~/.local/state/omarchy/phonelink/history/`.
Daily `YYYY-MM-DD.jsonl` files retain timestamped connection changes, scrcpy output, errors,
and recovery attempts. `phones.json` retains each known serial, manufacturer, model,
Android version and USB vendor/product identifier, plus each verified USB or wireless target.
Wireless records include IP, port, first-seen and last-seen timestamps. Previous endpoints
remain when a new one is discovered. Historical imports retain their source and do not
claim a new live verification. Metadata changes also appear in subsequent event snapshots.

The panel shows the latest forty events from the running service. Full history stays on disk;
there is no automatic deletion. History contains device/network identifiers but no phone
screen captures, messages, pairing codes or ADB private keys. Directories and files use
owner-only permissions. Back up the history folder if long-term retention matters.

## Build and install on Arch / Omarchy

```sh
./bin/build
./bin/test
./bin/install
```

The installer builds an Arch package, installs the launcher, SVG phone icons, binary and
systemd user unit, then enables the tray at graphical login. Set the phone serial in Settings.
Dependencies: Qt 6 base, declarative, SVG and Wayland, android-tools and scrcpy.
The source can live on another machine; compile the transferred source on the target Arch system.

```sh
systemctl --user enable --now phonelink.service
systemctl --user disable --now phonelink.service
```

Settings are stored in `~/.config/omarchy/phonelink.conf`. The launcher starts the user service
before opening the existing panel. No Omarchy or Hyprland window rules are required.
The app follows the Omarchy accent, rounding and language setting. English and Danish
catalogs are included; missing translations fall back to English.

## Per-user installation without sudo

```sh
./bin/install-user
```

This installs the binary under `~/.local/lib/phonelink/`, commands under `~/.local/bin/`,
icons and the launcher under the XDG user data directory, and the service under the XDG
user config directory. It enables the same graphical-login service without root access.
The Arch package remains available separately; this installation is not registered with pacman.
To switch to the system package, stop the user service, remove only these Phonelink user
installation files, reload systemd, then install the package and enable its service.
Keep the settings and history directories when switching installation methods.

## Commands for agents and terminals

```sh
phonelink status --json
phonelink start
phonelink stop
phonelink retry
phonelink restore-wireless
phonelink open
phonelink skill install --json
```

Status returns live JSON, including saved phone profiles. Control commands return 0 when
accepted and 1 on an IPC error; inspect status for the asynchronous operation result.
The skill installer writes a short guide under `${CODEX_HOME:-~/.codex}/skills/phonelink/`.
The CLI does not need a display. Start the user service first.

| Key | Action |
|---|---|
| Space | Open or stop screen, from Connection tab |
| Ctrl+R | Retry connection |
| Ctrl+B | Expand or collapse navigation |
| Q / Escape | Hide panel |
| ? | Show shortcuts |

## Verification

`bin/test` uses isolated settings and fake ADB/scrcpy executables. It covers endpoint validation,
phone identity rejection, persistent USB/wireless metadata, normal closure, crash recovery,
retry limits, USB bootstrapping, translations and the real QML panel. Live hardware and tray
integration must also be checked on the target desktop.

MIT licensed. Theme and translation support derive from the Omarchy app templates.
