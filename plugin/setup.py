#!/usr/bin/env python3
"""Confirmed, per-user source installation. No package manager or elevation is invoked."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid
sys.dont_write_bytecode = True
import bridge

ROOT = Path(__file__).resolve().parent.parent
TERMINAL = {"done", "failed", "cancelled", "interrupted"}
DEPENDENCIES = "omarchy pkg add base-devel qt6-base qt6-declarative qt6-svg qt6-wayland android-tools scrcpy python desktop-file-utils"

class Cancelled(Exception): pass

def cache_root():
    path=Path(os.environ.get("XDG_CACHE_HOME",Path.home()/".cache"))/"phonelink/service-setup"
    path.mkdir(parents=True,exist_ok=True,mode=0o700);path.chmod(0o700)
    return path

def save_json(path,value):
    path.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
    with tempfile.NamedTemporaryFile(mode="w",dir=path.parent,prefix=".state-",delete=False) as file:
        temporary=Path(file.name);json.dump(value,file)
    os.replace(temporary,path)

def read_json(path,default=None):
    try:
        if path.stat().st_size>65536:return default
        return json.loads(path.read_text())
    except (OSError,ValueError):return default

def locations():
    home=Path.home();data=Path(os.environ.get("XDG_DATA_HOME",home/".local/share"));config=Path(os.environ.get("XDG_CONFIG_HOME",home/".config"))
    return {"binary":home/".local/lib/phonelink/phonelink","command":home/".local/bin/phonelink","launcher":home/".local/bin/phonelink-launch",
            "desktop":data/"applications/phonelink.desktop","unit":config/"systemd/user/phonelink.service",
            **{name:data/f"icons/hicolor/scalable/apps/{name}.svg" for name in ("phonelink","phonelink-active","phonelink-offline")}}

def systemctl(*args):return bridge.run(["systemctl","--user",*args],timeout=5)

def expected_version():
    return read_json(ROOT/"manifest.json",{}).get("version","")

def preflight():
    paths=locations();backend=bridge.backend_path();unit=paths["unit"]
    missing=[tool for tool in ("qmake6","make","g++","pkg-config","systemctl") if not shutil.which(tool)]
    for name in ("adb","scrcpy"):
        if not Path("/usr/bin/"+name).is_file():missing.append(name)
    if "qmake6" not in missing:
        code,version=bridge.run(["qmake6","-query","QT_VERSION"])
        try:compatible=code==0 and tuple(int(x) for x in version.strip().split('.')[:2])>=(6,7)
        except ValueError:compatible=False
        if not compatible:missing.append("Qt >= 6.7")
    if "pkg-config" not in missing:
        code,_=bridge.run(["pkg-config","--exists","Qt6Quick","Qt6QuickControls2","Qt6DBus","Qt6Svg","Qt6WaylandClient"])
        if code:missing.append("Qt build modules")
    fragment=systemctl("show","phonelink.service","-p","FragmentPath","--value")[1].strip()
    mode="missing"
    if fragment.startswith(("/usr/","/etc/")) or backend and Path(backend) in (Path("/usr/bin/phonelink"),Path("/usr/local/bin/phonelink")):
        mode="system"
    elif backend or unit.exists():mode="user"
    allowed_commands={str(paths["binary"]),str(paths["command"])}
    if backend and str(Path(backend)) not in allowed_commands and mode!="system":mode="custom"
    if unit.exists():
        try:
            exec_lines=[s.strip() for s in unit.read_text().splitlines() if s.startswith("ExecStart=")]
            if exec_lines not in (["ExecStart=%h/.local/lib/phonelink/phonelink --tray"],[f"ExecStart={paths['binary']} --tray"]):mode="custom"
        except OSError:mode="custom"
    if any(p.is_symlink() for p in paths.values()) or list(unit.with_name(unit.name+".d").glob("*.conf")):mode="custom"
    version=bridge.run([backend,"--version"])[1].strip() if backend else ""
    active=systemctl("is-active","phonelink.service")[0]==0
    enabled=systemctl("is-enabled","phonelink.service")[1].strip() in ("enabled","enabled-runtime")
    service_ready=False
    if backend:
        code,out=bridge.run([backend,"status","--json"])
        try:
            api=json.loads(out)
            service_ready=code==0 and isinstance(api,dict) and api.get("pluginIntegration") is True
            if code==0 and isinstance(api,dict) and not active:mode="custom"
        except (ValueError,AttributeError):pass
    action="enable" if version in ("0.0.2", expected_version()) and not active and mode=="user" and unit.is_file() else "repair" if version==expected_version() and service_ready else "update" if mode=="user" else "install"
    blocked="managed" if mode in ("system","custom") else "dependencies" if (any(name in missing for name in ("systemctl", "adb", "scrcpy")) if action == "enable" else missing) else ""
    if systemctl("is-active","graphical-session.target")[0]!=0 and not blocked:blocked="session"
    return {"mode":mode,"version":version[:64],"targetVersion":expected_version(),"action":action,"active":active,"enabled":enabled if mode!="missing" else True,
            "missing":missing,"blocked":blocked,"canInstall":not blocked,"locations":[str(p) for p in paths.values()],"dependencyCommand":DEPENDENCIES,
            "instructions":"Install or update Phonelink from its bundled repository source. Review the detected installation and dependencies first. "
            "Bundled source: "+str(ROOT)+". Per-user build/install: ./bin/build && ./bin/test && systemctl --user stop phonelink.service && ./bin/install-user --no-build. "
            "For system/custom installs, update the existing installation instead of adding a competing copy. Missing dependencies: "+', '.join(missing)+". Dependency command for user approval: "+DEPENDENCIES}

def get_job(lock_held=False):
    pointer=read_json(cache_root()/"latest.json",{})
    identifier=pointer.get("id","")
    if len(identifier)!=32 or any(c not in "0123456789abcdef" for c in identifier):return {"phase":"idle","log":""}
    state=read_json(cache_root()/identifier/"status.json",{"phase":"failed","log":"Could not read installation status."})
    state["id"]=identifier
    state["directory"]=str(cache_root()/identifier)
    if state.get("phase") not in TERMINAL:
        with (cache_root()/"install.lock").open("a+") as lock:
            try:
                if not lock_held:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                state["phase"]="interrupted";state["log"]="The installer stopped unexpectedly. Backups are retained in the job folder; review before retrying.\n"+state.get("log","")
            except BlockingIOError:pass
    return state

def begin(confirmed,enable_login):
    if not confirmed:return {"ok":False,"error":"confirmation"}
    root=cache_root();lock=(root/"install.lock").open("a+")
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:lock.close();return {"ok":False,"error":"busy","job":get_job()}
    previous=get_job(lock_held=True)
    if previous.get("id") and (root/previous["id"]/"transaction.json").exists():
        lock.close();return {"ok":False,"error":"recovery","job":previous}
    plan=preflight()
    if not plan["canInstall"]:lock.close();return {"ok":False,"error":plan["blocked"],"plan":plan}
    identifier=uuid.uuid4().hex;job=root/identifier;job.mkdir(mode=0o700)
    save_json(job/"request.json",{"enableLogin":enable_login,"plan":plan})
    save_json(job/"status.json",{"phase":"queued","log":"Preparing a private source build.\n"})
    save_json(root/"latest.json",{"id":identifier})
    try:
        with (job/"worker.log").open("ab") as log:
            subprocess.Popen([sys.executable,"-B",str(ROOT/"plugin/setup.py"),"worker","--job",identifier,"--lock-fd",str(lock.fileno())],pass_fds=(lock.fileno(),),stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
    except OSError:
        save_json(job/"status.json",{"phase":"failed","log":"Could not start installer worker."});lock.close();return {"ok":False,"error":"worker"}
    lock.close();return {"ok":True,"job":{"id":identifier,"phase":"queued","log":"Preparing a private source build.\n"}}

class Job:
    def __init__(self,path):self.path=path;self.log="";self.phase="queued"
    def write(self,phase=None,text=""):
        if phase:self.phase=phase
        self.log=(self.log+text)[-14000:]
        save_json(self.path/"status.json",{"phase":self.phase,"log":self.log})
    def cancelled(self):return (self.path/"cancel").exists()
    def command(self,args,timeout=30,check=True):
        if self.cancelled():raise Cancelled()
        with tempfile.TemporaryFile() as output:
            process=subprocess.Popen(args,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
            started=time.monotonic();position=0
            try:
                while True:
                    code=process.poll()
                    data=os.pread(output.fileno(),8192,position);position+=len(data)
                    if data:self.write(text=data.decode("utf-8",errors="replace"))
                    if self.cancelled():raise Cancelled()
                    if time.monotonic()-started>timeout:raise RuntimeError("Command timed out: "+args[0])
                    if code is not None:
                        remaining=os.pread(output.fileno(),14000,position)
                        if remaining:self.write(text=remaining.decode("utf-8",errors="replace"))
                        if check and code!=0:raise RuntimeError("Command failed: "+args[0])
                        return code
                    time.sleep(.2)
            finally:
                if process.poll() is None:
                    os.killpg(process.pid,signal.SIGTERM)
                    try:process.wait(timeout=2)
                    except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()

    def snapshot(self):
        backup=self.path/"backup";backup.mkdir(mode=0o700)
        records=[]
        paths=locations();paths["unit-backup"]=paths["unit"].with_suffix(".service.bak")
        for key,target in paths.items():
            if target.is_symlink():raise RuntimeError("Installation path changed to a symlink; refusing replacement.")
            exists=target.exists()
            if exists:shutil.copy2(target,backup/key)
            records.append({"key":key,"path":str(target),"existed":exists})
        tx={"files":records,"active":systemctl("is-active","phonelink.service")[0]==0,"enabled":systemctl("is-enabled","phonelink.service")[1].strip() in ("enabled","enabled-runtime")}
        backend=bridge.backend_path()
        tx["mirroring"]=False
        if tx["active"] and backend:
            code,out=bridge.run([backend,"status","--json"])
            try:tx["mirroring"]=code==0 and bool(json.loads(out).get("running"))
            except (ValueError,AttributeError):pass
        save_json(self.path/"transaction.json",tx);return tx
    def restore(self,tx):
        self.write("recovering","Restoring the previous installation.\n")
        # Recovery must complete even after a cancel request.
        (self.path/"cancel").unlink(missing_ok=True)
        if systemctl("stop","phonelink.service")[0]!=0 and systemctl("is-active","phonelink.service")[0]==0:
            raise RuntimeError("Could not stop replacement service; backups retained.")
        # Remove new enablement before restoring a previously absent unit.
        if locations()["unit"].exists() and systemctl("disable","phonelink.service")[0]!=0:
            raise RuntimeError("Could not clear replacement startup; backups retained.")
        for item in tx["files"]:
            target=Path(item["path"])
            if item["existed"]:
                target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(self.path/"backup"/item["key"],target)
            else:target.unlink(missing_ok=True)
        if systemctl("daemon-reload")[0]!=0:raise RuntimeError("Could not reload restored service; backups retained.")
        if systemctl("enable" if tx["enabled"] else "disable","phonelink.service")[0]!=0 and locations()["unit"].exists():raise RuntimeError("Could not restore login startup; backups retained.")
        if tx["active"]:
            if systemctl("start","phonelink.service")[0]!=0:raise RuntimeError("Could not restart the previous service; backups retained.")
        if tx.get("mirroring") and tx["active"]:bridge.run([str(locations()["binary"]),"start"])
        (self.path/"transaction.json").unlink(missing_ok=True)

def copy_source(source,target):
    def ignore(directory,names):
        return [name for name in names if (name in (".git","__pycache__") or (name == "build" and (Path(directory)/name).is_dir())) or name.endswith((".pyc",".pkg.tar.zst"))]
    for current,dirs,files in os.walk(source):
        dirs[:]=[d for d in dirs if d not in (".git","build","__pycache__")]
        if any((Path(current)/name).is_symlink() for name in dirs+files):raise RuntimeError("Bundled source contains a symlink.")
    shutil.copytree(source,target,ignore=ignore)

def verify_service(path,cancelled=lambda:False):
    for attempt in range(10):
        if cancelled():raise Cancelled()
        code,out=bridge.run([str(path),"status","--json"])
        try:
            if code==0 and json.loads(out).get("pluginIntegration") is True:return True
        except (ValueError,AttributeError):pass
        time.sleep(.3)
    return False

def worker(identifier,lock_fd):
    if len(identifier)!=32 or any(c not in "0123456789abcdef" for c in identifier):return 1
    job=Job(cache_root()/identifier);request=read_json(job.path/"request.json",{});tx=None
    try:
        # Holding the inherited descriptor keeps installs serialized across shell restarts.
        os.fstat(lock_fd)
        plan=preflight()
        if not plan["canInstall"]:raise RuntimeError("Preflight changed: "+plan["blocked"])
        if plan["action"]=="enable":
            tx=job.snapshot();job.write("starting","Starting the installed user service.\n")
        else:
            job.write("building","Building bundled source; the current service is still available.\n")
            stage=job.path/"source";copy_source(ROOT,stage)
            job.command([str(stage/"bin/build")],timeout=900)
            code,built_version=bridge.run([str(stage/"build/phonelink"),"--version"])
            if code or built_version.strip()!=expected_version():raise RuntimeError("Built backend version does not match the plugin manifest.")
            if job.cancelled():raise Cancelled()
            tx=job.snapshot();job.write("installing","Installing per-user binary, icons, launcher and service.\n")
            job.command(["systemctl","--user","stop","phonelink.service"],check=tx["active"])
            job.command([sys.executable,"-B",str(stage/"bin/install-user"),"--no-build","--no-service"],timeout=60)
        job.command(["systemctl","--user","enable" if request.get("enableLogin") else "disable","phonelink.service"])
        job.command(["systemctl","--user","start","phonelink.service"])
        job.write("verifying","Verifying service communication.\n")
        if not verify_service(locations()["binary"],job.cancelled):raise RuntimeError("The installed service did not respond correctly.")
        if tx and tx.get("mirroring"):job.command([str(locations()["binary"]),"start"])
        (job.path/"transaction.json").unlink(missing_ok=True)
        job.write("done","Service installed and verified. Phone debugging approvals remain a separate step.\n")
        return 0
    except (Exception,KeyboardInterrupt) as error:
        cancelled=isinstance(error,(Cancelled,KeyboardInterrupt))
        if tx:
            try:job.restore(tx)
            except Exception as recovery:job.write("failed",str(recovery)+"\nOriginal error: "+str(error)+"\n");return 1
        job.write("cancelled" if cancelled else "failed",("Installation cancelled.\n" if cancelled else str(error)+"\n"));return 1
    finally:
        os.close(lock_fd)

def cancel():
    state=get_job()
    if state.get("phase") not in TERMINAL and state.get("id"):
        (cache_root()/state["id"]/"cancel").touch(mode=0o600)
    return {"ok":True,"job":get_job()}

def main():
    parser=argparse.ArgumentParser();parser.add_argument("action",choices=["inspect","begin","status","cancel","worker"])
    parser.add_argument("--confirmed",action="store_true");parser.add_argument("--login",choices=["yes","no"],default="yes")
    parser.add_argument("--job");parser.add_argument("--lock-fd",type=int)
    args=parser.parse_args()
    if args.action=="worker":return worker(args.job,args.lock_fd)
    try:
        response={"plan":preflight(),"job":get_job()} if args.action=="inspect" else begin(args.confirmed,args.login=="yes") if args.action=="begin" else cancel() if args.action=="cancel" else {"job":get_job()}
    except Exception as error:response={"ok":False,"error":"setup","message":str(error)[:1000]}
    print(json.dumps(response,separators=(",",":")));return 0

if __name__=="__main__":sys.exit(main())
