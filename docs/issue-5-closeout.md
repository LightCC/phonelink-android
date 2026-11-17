# Issue 5 closeout — 2026-11-16

The native plugin now offers confirmed per-user service installation from bundled
source. No separate server download is required. Setup reports ownership/version,
dependencies and graphical-session readiness, shows affected paths and login startup,
and offers progress, bounded build details, cancel/retry and user-copied agent guidance.
It does not invoke a package manager, request sudo, automatically contact an agent,
replace system/custom installations or configure phone debugging.

A detached worker holds an exclusive flock, copies source into a private cache and
builds before stopping the current service. It backs up owned files/service state,
installs with --no-service, applies the chosen startup policy and verifies IPC.
Normal installation/verification failures and cancellation roll back the replacement.
An interrupted or failed recovery retains its transaction and blocks another install.
A stopped compatible installation can be started without a build toolchain.

## Verification on AI Evo

- Manifest validation and qmllint pass for both QML entry points.
- 31 Python tests pass: 10 bridge tests plus 21 installer tests. Coverage includes
  confirmation, dependencies/old Qt, system/custom/unsupervised ownership, stopped
  compatible backend without build tools, concurrency, retained interrupted/failed
  transactions, source symlinks, keeping bin/build while excluding artifacts,
  timeout/cancel, file/service rollback, failed first-install cleanup, partial install
  failure and cancellation, verification failure and startup choice.
- 16 Qt tests pass, including offscreen panel interaction and existing connection,
  tray lease, history, recovery and startup settings coverage. No C++ behavior changed
  beyond version metadata. The test D-Bus emitted a portal warning; no test failed.
- Live build cancellation left backend 0.0.2 active; a concurrent begin was rejected.
- Live guided update built from the installed plugin clone, kept 0.0.2 active during
  compilation, installed 0.0.3 and passed IPC verification with login startup enabled.
  Wireless debugging remained connected and mirroring stayed stopped. Existing config,
  history and ADB keys were not installation targets.
- Stopped 0.0.3 was detected as a start-only action and restarted without rebuilding.
- A second confirmed build continued while the Omarchy shell restarted. Reopening the
  native setup panel recovered the same active job ID/progress; the job then completed.
- Native setup was visually checked in English and Danish using the actual Omarchy
  shell, and language state was restored. Dynamic text/logs use plain text. Setup IPC
  open/close worked; the installed desktop entry passes desktop-file-validate.

## Limits and operations

No fresh desktop login, new machine, real system-package replacement or power-loss
recovery was exercised. Missing-install and failure recovery are tested with isolated
HOME/XDG fixtures and fake services. Physical mirroring was not started during this
installation work. Multi-monitor setup-panel IPC routing has not been manually tested.
Plugin removal/lease fallback was checked under issue 4; this change installs the
backend into independent per-user paths and stages source outside the plugin directory.
A forced kill during replacement needs manual/agent-assisted review of retained job
backups and transaction state. Completed source builds/backups remain in the private
cache until the user removes them. Do not delete an active/interrupted job.

Hot reload of several changed plugin files did not recreate the widget reliably in
this shell session; restarting the shell loaded the final panel successfully. No
packaged Omarchy file was modified. The standalone backend continued running through
shell restarts. This PR does not publish the plugin to the marketplace.
