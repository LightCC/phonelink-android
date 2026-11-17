import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).parents[1]))
spec=importlib.util.spec_from_file_location("setup_service",Path(__file__).parents[1]/"setup.py")
setup=importlib.util.module_from_spec(spec);spec.loader.exec_module(setup)

class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.base=Path(self.temp.name)
        for name in ("home","config","data","cache","source"): (self.base/name).mkdir()
        self.addCleanup(patch.stopall)
        patch.object(Path,"home",return_value=self.base/"home").start()
        patch.dict(os.environ,{"XDG_CONFIG_HOME":str(self.base/"config"),"XDG_DATA_HOME":str(self.base/"data"),"XDG_CACHE_HOME":str(self.base/"cache")}).start()
        patch.object(setup,"ROOT",self.base/"source").start()
        (setup.ROOT/"manifest.json").write_text('{"version":"0.0.3"}')
    def service(self,*args):
        if args[:1]==("is-active",):return (0,"active")
        if args[:1]==("is-enabled",):return (0,"enabled")
        return (0,"")
    def plan(self,action="update"):
        return {"canInstall":True,"blocked":"","mode":"user","action":action}
    def make_job(self):
        identifier="a"*32;path=setup.cache_root()/identifier;path.mkdir()
        setup.save_json(path/"request.json",{"enableLogin":False,"plan":self.plan()})
        setup.save_json(path/"status.json",{"phase":"queued","log":""})
        descriptor=os.open(setup.cache_root()/"install.lock",os.O_RDWR|os.O_CREAT,0o600)
        return identifier,path,descriptor
    def test_confirmation_is_required(self):
        with patch.object(setup.subprocess,"Popen") as process:
            self.assertEqual(setup.begin(False,True)["error"],"confirmation");process.assert_not_called()
    def test_missing_dependencies_block_install(self):
        with patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None),patch.object(setup.shutil,"which",return_value=None):
            plan=setup.preflight();self.assertFalse(plan["canInstall"]);self.assertEqual(plan["blocked"],"dependencies");self.assertIn("qmake6",plan["missing"])
    def test_system_install_is_not_replaced(self):
        def service(*args):return (0,"/usr/lib/systemd/user/phonelink.service") if args[:1]==("show",) else self.service(*args)
        with patch.object(setup,"systemctl",side_effect=service),patch.object(setup.bridge,"backend_path",return_value="/usr/bin/phonelink"),patch.object(setup.shutil,"which",return_value=None),patch.object(setup.bridge,"run",return_value=(0,"0.0.2")):
            plan=setup.preflight();self.assertEqual(plan["mode"],"system");self.assertFalse(plan["canInstall"])
    def test_custom_unit_is_not_overwritten(self):
        unit=setup.locations()["unit"];unit.parent.mkdir(parents=True);unit.write_text("ExecStart=/other/app")
        with patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None),patch.object(setup.shutil,"which",return_value=None):
            self.assertEqual(setup.preflight()["mode"],"custom")
    def test_qt_too_old_is_blocked(self):
        def run(args,*a,**kw):return (0,"6.4.2") if args[0]=="qmake6" else (0,"")
        with patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None),patch.object(setup.shutil,"which",return_value="/tool"),patch.object(setup.bridge,"run",side_effect=run):
            self.assertIn("Qt >= 6.7",setup.preflight()["missing"])
    def test_stopped_compatible_service_needs_no_build_tools(self):
        paths=setup.locations();paths["unit"].parent.mkdir(parents=True);paths["unit"].write_text("ExecStart=%h/.local/lib/phonelink/phonelink --tray")
        def service(*args):
            return (1,"inactive") if args==("is-active","phonelink.service") else self.service(*args)
        def run(args,*a,**kw):return (0,"0.0.2") if "--version" in args else (1,"")
        with patch.object(setup,"systemctl",side_effect=service),patch.object(setup.bridge,"backend_path",return_value=str(paths["binary"])),patch.object(setup.shutil,"which",side_effect=lambda name: "/tool" if name=="systemctl" else None),patch.object(setup.bridge,"run",side_effect=run):
            # Only build tools are missing; runtime dependencies exist in this fixture.
            with patch.object(setup.Path,"is_file",return_value=True):
                plan=setup.preflight();self.assertEqual(plan["action"],"enable");self.assertTrue(plan["canInstall"])
    def test_unsupervised_running_backend_is_not_replaced(self):
        def service(*args):return (1,"inactive") if args==("is-active","phonelink.service") else self.service(*args)
        def run(args,*a,**kw):return (0,"0.0.3") if "--version" in args else (0,'{"pluginIntegration":true}')
        with patch.object(setup,"systemctl",side_effect=service),patch.object(setup.bridge,"backend_path",return_value=str(setup.locations()["binary"])),patch.object(setup.shutil,"which",return_value=None),patch.object(setup.bridge,"run",side_effect=run):
            self.assertEqual(setup.preflight()["mode"],"custom")
    def test_concurrent_install_is_rejected(self):
        lock=(setup.cache_root()/"install.lock").open("a+")
        try:
            setup.fcntl.flock(lock,setup.fcntl.LOCK_EX|setup.fcntl.LOCK_NB)
            self.assertEqual(setup.begin(True,True)["error"],"busy")
        finally:lock.close()
    def test_snapshot_restores_files_and_service_state(self):
        target=setup.locations()["binary"];target.parent.mkdir(parents=True);target.write_bytes(b"old")
        path=setup.cache_root()/"job";path.mkdir();job=setup.Job(path)
        with patch.object(setup,"systemctl",side_effect=self.service) as service,patch.object(setup.bridge,"backend_path",return_value=None):
            transaction=job.snapshot();target.write_bytes(b"new");job.restore(transaction)
            self.assertEqual(target.read_bytes(),b"old");self.assertIn(("start","phonelink.service"),[c.args for c in service.call_args_list])
    def test_failed_first_install_removes_new_unit_and_enablement(self):
        path=setup.cache_root()/"job";path.mkdir();job=setup.Job(path)
        with patch.object(setup,"systemctl",side_effect=self.service) as service,patch.object(setup.bridge,"backend_path",return_value=None):
            tx=job.snapshot();unit=setup.locations()["unit"];unit.parent.mkdir(parents=True);unit.write_text("new unit")
            job.restore(tx);self.assertFalse(unit.exists())
            self.assertIn(("disable","phonelink.service"),[c.args for c in service.call_args_list])
    def test_build_cancel_does_not_stop_service(self):
        identifier,path,fd=self.make_job()
        with patch.object(setup,"preflight",return_value=self.plan()),patch.object(setup,"copy_source"),patch.object(setup.Job,"command",side_effect=setup.Cancelled()),patch.object(setup,"systemctl") as service:
            self.assertEqual(setup.worker(identifier,fd),1);service.assert_not_called()
        self.assertEqual(setup.read_json(path/"status.json")["phase"],"cancelled")
    def test_failed_verification_rolls_back(self):
        identifier,path,fd=self.make_job();target=setup.locations()["binary"];target.parent.mkdir(parents=True);target.write_bytes(b"old")
        def command(job,args,*a,**kw):
            if any("install-user" in str(arg) for arg in args):target.write_bytes(b"new")
            return 0
        with patch.object(setup,"preflight",return_value=self.plan()),patch.object(setup,"copy_source"),patch.object(setup.bridge,"run",return_value=(0,"0.0.3")),patch.object(setup.Job,"command",autospec=True,side_effect=command),patch.object(setup,"verify_service",return_value=False),patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None):
            self.assertEqual(setup.worker(identifier,fd),1)
        self.assertEqual(target.read_bytes(),b"old");self.assertEqual(setup.read_json(path/"status.json")["phase"],"failed")
    def test_install_failure_and_cancel_after_replacement_restore_previous_files(self):
        for error in (RuntimeError("install failed"),setup.Cancelled()):
            with self.subTest(error=type(error).__name__):
                identifier,path,fd=self.make_job();target=setup.locations()["binary"];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b"old")
                def command(job,args,*a,**kw):
                    if any("install-user" in str(arg) for arg in args):
                        target.write_bytes(b"partly installed");raise error
                    return 0
                with patch.object(setup,"preflight",return_value=self.plan()),patch.object(setup,"copy_source"),patch.object(setup.bridge,"run",return_value=(0,"0.0.3")),patch.object(setup.Job,"command",autospec=True,side_effect=command),patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None):
                    self.assertEqual(setup.worker(identifier,fd),1)
                self.assertEqual(target.read_bytes(),b"old");self.assertFalse((path/"transaction.json").exists())
                import shutil
                shutil.rmtree(path)
    def test_enable_only_skips_build_and_respects_login_choice(self):
        identifier,path,fd=self.make_job()
        with patch.object(setup,"preflight",return_value=self.plan("enable")),patch.object(setup,"copy_source") as copy,patch.object(setup.Job,"command",return_value=0) as command,patch.object(setup,"verify_service",return_value=True),patch.object(setup,"systemctl",side_effect=self.service),patch.object(setup.bridge,"backend_path",return_value=None):
            self.assertEqual(setup.worker(identifier,fd),0);copy.assert_not_called()
            self.assertIn(["systemctl","--user","disable","phonelink.service"],[call.args[0] for call in command.call_args_list])
        self.assertEqual(setup.read_json(path/"status.json")["phase"],"done")
    def test_status_survives_client_restart_and_detects_interruption(self):
        identifier,path,fd=self.make_job();setup.save_json(setup.cache_root()/"latest.json",{"id":identifier});os.close(fd)
        self.assertEqual(setup.get_job()["phase"],"interrupted")
    def test_interrupted_replacement_requires_recovery(self):
        identifier,path,fd=self.make_job();setup.save_json(setup.cache_root()/"latest.json",{"id":identifier});setup.save_json(path/"transaction.json",{});os.close(fd)
        self.assertEqual(setup.begin(True,True)["error"],"recovery")
    def test_failed_rollback_also_blocks_new_install(self):
        identifier,path,fd=self.make_job();setup.save_json(setup.cache_root()/"latest.json",{"id":identifier})
        setup.save_json(path/"status.json",{"phase":"failed","log":"Recovery failed"});setup.save_json(path/"transaction.json",{});os.close(fd)
        self.assertEqual(setup.begin(True,True)["error"],"recovery")
    def test_source_snapshot_keeps_build_script_and_excludes_artifacts(self):
        (setup.ROOT/"bin").mkdir();(setup.ROOT/"bin/build").write_text("#!/bin/sh")
        (setup.ROOT/"build").mkdir();(setup.ROOT/"build/artifact").write_bytes(b"binary")
        stage=self.base/"stage";setup.copy_source(setup.ROOT,stage)
        self.assertTrue((stage/"bin/build").is_file());self.assertFalse((stage/"build").exists())
    def test_source_snapshot_rejects_symlinks(self):
        (setup.ROOT/"link").symlink_to(self.base/"home")
        with self.assertRaises(RuntimeError):setup.copy_source(setup.ROOT,self.base/"stage")
    def test_cancel_before_command_launch(self):
        path=setup.cache_root()/"job";path.mkdir();job=setup.Job(path);(path/"cancel").touch()
        with self.assertRaises(setup.Cancelled):job.command([sys.executable,"-c","raise SystemExit(99)"])
    def test_command_timeout_terminates_child(self):
        path=setup.cache_root()/"job";path.mkdir();job=setup.Job(path)
        with self.assertRaisesRegex(RuntimeError,"timed out"):job.command([sys.executable,"-c","import time;time.sleep(2)"],timeout=.05)

if __name__=="__main__":unittest.main()
