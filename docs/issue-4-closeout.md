# Issue 4 closeout — 2026-11-16

The root manifest declares io.github.lightcc.phonelink as a bar-widget, with a native
QML entry point and host theme/language components. The widget opens the existing
tiled application. It does not implement a separate native popup. Missing, outdated,
stopped and failed backend states produce owned, actionable tooltip/notification text.

The small Python bridge uses bounded subprocesses and response limits, and only
forwards boolean status fields to the shell. It starts an installed user service
only on explicit activation; it does not install packages or run ADB itself.
Backend 0.0.2 advertises pluginIntegration and implements a short runtime lease that
suppresses the generic tray while the widget is refreshing. Standalone tray access
returns after lease expiry when the widget is disabled or removed. Service and data
ownership remain separate. Guided installation/upgrade belongs to issue 5.

## Checks performed on AI Evo

Manifest validation and qmllint against the installed Omarchy shell pass.
Ten Python bridge tests pass, including absent/old/stopped backend, service-start
failure, state filtering, timeout, output-size limits, permissions and lease failure.
Sixteen Qt backend tests pass, including valid/expired/future lease handling and
existing tray/connection/recovery/UI coverage. Qt tests now use an isolated D-Bus
session so their tray fixture cannot register with the real desktop.

Live checks used Omarchy 4.5.0 and Qt 6.12. The backend was updated in place without
losing phone configuration/history. The widget appeared as one phone icon beside
the network controls, while the generic tray became Passive. The shell summon/open
route opened the existing panel. Disabling the widget restored Active standalone
tray status after natural lease expiry, with the backend still running.

Removed the initial unpacked development copy using omarchy plugin remove. Then
installed the committed feature branch through omarchy plugin add using the Mirror
Git worktree's SSH URL. This exercised the actual clone/manifest/enable path without
merging unfinished work into GitHub's default branch. The installed plugin follows
the feature branch. The shell was restarted; the plugin remained enabled and resumed
status updates while the backend stayed active. Plugin-manager Git update was also
checked after the closeout commit. The plugin supports removal without deleting
phone data or uninstalling the backend.

## Limits

No fresh desktop login, new physical machine, marketplace submission or non-Omarchy
shell was tested. The backend must already be installed; issue 5 owns guided delivery.
The published default-branch plugin-add URL becomes usable after this PR is merged.
The native plugin uses Python 3 and the installed Omarchy shell API. Runtime lease
expiry restores the standalone tray within 25 seconds plus the one-second check.
