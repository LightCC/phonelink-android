---
name: phonelink
description: Inspect and manage the local Phonelink tray service and USB or wireless scrcpy connections.
---

Run `phonelink status --json` for live connection state and saved phone profiles.
Use `phonelink start`, `phonelink stop`, or `phonelink retry` for screen control.
Use `phonelink restore-wireless` only when the authorized phone is connected through USB.
It toggles modern Android wireless debugging; it does not enable legacy TCP port 5555.
Use `systemctl --user status phonelink.service` and `journalctl --user -u phonelink.service` for service diagnostics.
History is under `$XDG_STATE_HOME/omarchy/phonelink/history` (default `~/.local/state/omarchy/phonelink/history`).
`phones.json` retains verified phone identities and previous endpoints; daily JSONL files record events.
Do not replace or delete existing ADB keys. Do not pair a new phone without user authorization.
If pairing requires a secret, use the host assistant secure secret tool rather than asking for it in chat.
