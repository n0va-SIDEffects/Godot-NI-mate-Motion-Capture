#!/root/.local/share/uv/tools/pebble-tool/bin/python
# Run with the pebble-tool venv python: <uv tools dir>/pebble-tool/bin/python install_sdk_local.py core.tar.gz toolchain.tar.gz
"""Install a Pebble SDK core + toolchain from locally downloaded tarballs,
replicating pebble_tool's SDKManager._install_from_handle without the
(slow) in-process downloads."""
import json, os, platform, shutil, subprocess, sys, tarfile
from contextlib import closing
from pebble_tool.sdk.manager import SDKManager
from pebble_tool.util.npm import invoke_npm

core_tar, toolchain_tar = sys.argv[1], sys.argv[2]
mgr = SDKManager()
with tarfile.open(core_tar, "r:*") as t:
    with closing(t.extractfile("sdk-core/manifest.json")) as f:
        info = json.load(f)
    version = info["version"]
    path = os.path.join(mgr.sdk_dir, version)
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)
    t.extractall(path)
print("extracted core", version, "->", path)
venv = os.path.join(path, ".venv")
subprocess.check_call([sys.executable, "-m", "venv", venv])
subprocess.check_call([os.path.join(venv, "bin", "python"), "-m", "pip", "install", "-q", "-r",
                       os.path.join(path, "sdk-core", "requirements.txt")])
pkg = os.path.join(path, "sdk-core", "package.json")
if os.path.exists(pkg):
    os.mkdir(os.path.join(path, "node_modules"))
    shutil.copy2(pkg, os.path.join(path, "package.json"))
    invoke_npm(["install", "--silent"], cwd=path)
mgr.set_current_sdk(version)
with open(toolchain_tar, "rb") as f:
    mgr._install_toolchain_from_handle(f, version, "linux-" + platform.machine())
print("done")
