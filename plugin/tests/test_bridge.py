import importlib.util
import json
import os
from pathlib import Path
import tempfile
import time
import sys
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("bridge", Path(__file__).parents[1]/"bridge.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

class BridgeTests(unittest.TestCase):
    def test_command_timeout_is_bounded(self):
        code,output=bridge.run([sys.executable,"-c","import time; time.sleep(2)"],timeout=.05)
        self.assertEqual((code,output),(1,""))
    def test_oversized_output_is_rejected(self):
        code,output=bridge.run([sys.executable,"-c","print('x'*300000)"],timeout=2)
        self.assertEqual((code,output),(1,""))
    def test_no_backend_is_actionable(self):
        with patch.object(bridge, "backend_path", return_value=None):
            self.assertEqual(bridge.status(), {"available": False, "code": "missing"})
    def test_stopped_backend_does_not_spawn_unsupervised_app(self):
        with patch.object(bridge,"backend_path",return_value="/example/phonelink"), patch.object(bridge,"run",return_value=(1,"")) as run, patch.object(bridge.shutil,"which",return_value=None):
            self.assertEqual(bridge.open_panel()["code"],"serviceError")
            self.assertNotIn(["/example/phonelink","open"],[call.args[0] for call in run.call_args_list])
    def test_old_backend_requires_upgrade(self):
        with patch.object(bridge,"backend_path",return_value="/example/phonelink"), patch.object(bridge,"run",return_value=(0,'{"running":false}')):
            self.assertEqual(bridge.status()["code"],"upgrade")
    def test_private_or_markup_fields_are_not_forwarded(self):
        state={"pluginIntegration":True,"running":False,"usbReady":True,"wirelessReady":False,"serial":"SECRET","profiles":{"private":True},"model":"<img src=https://invalid/>"}
        with patch.object(bridge,"backend_path",return_value="/example/phonelink"), patch.object(bridge,"run",return_value=(0,json.dumps(state))), patch.object(bridge,"renew_lease",return_value=True):
            result=bridge.status();self.assertTrue(result["available"])
            self.assertEqual(set(result["state"]),{"pluginIntegration","running","usbReady","wirelessReady"})
    def test_invalid_boolean_state_is_rejected(self):
        with self.assertRaises(ValueError):
            bridge.sanitize_state({"pluginIntegration":True,"running":"true","usbReady":False,"wirelessReady":False})
    def test_lease_is_short_and_owner_only(self):
        with tempfile.TemporaryDirectory() as directory,patch.dict(os.environ,{"XDG_RUNTIME_DIR":directory}):
            self.assertTrue(bridge.renew_lease());path=bridge.lease_path()
            self.assertTrue(20<=int(path.read_text())-int(time.time())<=25)
            self.assertEqual(path.stat().st_mode&0o777,0o600)
    def test_status_lease_failure_does_not_claim_available(self):
        state={"pluginIntegration":True,"running":False,"usbReady":True,"wirelessReady":False}
        with patch.object(bridge,"backend_path",return_value="/example/phonelink"), patch.object(bridge,"run",return_value=(0,json.dumps(state))), patch.object(bridge,"renew_lease",return_value=False):
            self.assertEqual(bridge.status()["code"],"runtime")
    def test_open_uses_service_then_existing_panel(self):
        stopped={"available":False,"code":"stopped"};ready={"available":True,"code":"ready","state":{}}
        with patch.object(bridge,"backend_path",return_value="/example/phonelink"), patch.object(bridge,"status",side_effect=[stopped,ready]),patch.object(bridge,"run",return_value=(0,"")) as run:
            self.assertTrue(bridge.open_panel()["available"])
            self.assertEqual(run.call_args_list[0].args[0],["systemctl","--user","start","phonelink.service"])
            self.assertEqual(run.call_args_list[1].args[0],["/example/phonelink","open"])

if __name__=="__main__":unittest.main()
