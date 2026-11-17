#!/usr/bin/env python3
"""Bounded IPC adapter. This helper never installs packages or runs ADB itself."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

LIMIT = 262144

def run(args, timeout=4):
    # Spill large command output to a temporary file, keeping it outside the shell process.
    try:
        with tempfile.TemporaryFile() as output:
            result = subprocess.run(args, stdout=output, stderr=subprocess.DEVNULL, timeout=timeout)
            output.seek(0)
            data = output.read(LIMIT + 1)
            if len(data) > LIMIT:
                return 1, ""
            return result.returncode, data.decode("utf-8", errors="replace")
    except (OSError, subprocess.TimeoutExpired):
        return 1, ""

def backend_path():
    candidates = [shutil.which("phonelink"), str(Path.home()/".local/lib/phonelink/phonelink"), "/usr/bin/phonelink"]
    for candidate in candidates:
        if candidate and Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None

def lease_path():
    return Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}"))/"phonelink-plugin-active"

def renew_lease():
    target = lease_path()
    if not target.parent.is_dir():
        return False
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=target.parent, prefix=".phonelink-lease-", delete=False) as file:
            temporary = Path(file.name)
            file.write(str(int(time.time()) + 25))
        os.replace(temporary, target)
        return True
    except OSError:
        if "temporary" in locals():
            temporary.unlink(missing_ok=True)
        return False

def sanitize_state(state):
    if not isinstance(state, dict):
        raise ValueError("Invalid state")
    required = ("usbReady", "wirelessReady", "running", "pluginIntegration")
    if any(type(state.get(key)) is not bool for key in required):
        raise ValueError("Unsupported state contract")
    # No device identifiers, history or untrusted strings enter tooltips or the shell.
    return {key: state[key] for key in required}

def status():
    path = backend_path()
    if not path:
        return {"available": False, "code": "missing"}
    code, output = run([path, "status", "--json"])
    if code:
        return {"available": False, "code": "stopped"}
    try:
        state = json.loads(output)
        if not isinstance(state, dict) or state.get("pluginIntegration") is not True:
            return {"available": False, "code": "upgrade"}
        sanitized = sanitize_state(state)
    except (ValueError, TypeError):
        return {"available": False, "code": "unavailable"}
    if not renew_lease():
        return {"available": False, "code": "runtime"}
    return {"available": True, "code": "ready", "state": sanitized}

def open_panel(title="Phonelink", notice="Install the Phonelink backend using bin/install-user, then enable phonelink.service."):
    state = status()
    path = backend_path()
    if path and state["code"] == "stopped":
        code, _ = run(["systemctl", "--user", "start", "phonelink.service"], 6)
        if code == 0:
            state = status()
        else:
            state = {"available": False, "code": "serviceError"}
    if state["available"] and path:
        code, _ = run([path, "open"])
        if code:
            return {"available": False, "code": "unavailable"}
        return state
    # Notification text comes from the plugin's own catalogs, never backend output.
    if shutil.which("notify-send"):
        run(["notify-send", "--app-name=Phonelink", title[:128], notice[:1000]])
    return state

def main(argv):
    action = argv[1] if len(argv) > 1 else "status"
    if action == "status":
        response = status()
    elif action == "open":
        response = open_panel(*(argv[2:4]))
    else:
        response = {"available": False, "code": "unavailable"}
    print(json.dumps(response, separators=(",", ":")))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
