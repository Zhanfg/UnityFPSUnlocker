#!/usr/bin/env python3
"""Simulate KernelSU module installation, not merely ZIP mode bits.

KernelSU's default extraction normally resets file permissions to 0644.
This reproduces the v0.7.0 "ERROR: profile helper missing" regression.
"""
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile
import os
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: verify_installed_module.py MODULE.zip")

zip_path = Path(sys.argv[1])
executables = (
    "bin/fps-profilectl", "bin/profile-ui.sh", "service.sh",
    "post-fs-data.sh", "perf-guard.sh", "opgce-bridge.sh",
    "scrc-bridge.sh", "sf-present-fps.sh", "fps-evidence.sh",
    "game-diag.sh",
    "fengchi-probe.sh",
)
with TemporaryDirectory(prefix="ksu-install-") as tmp:
    module = Path(tmp) / "unityfpsunlocker_oemsafe"
    module.mkdir()
    with ZipFile(zip_path) as z:
        bad = [x.filename for x in z.infolist() if
               x.filename.startswith("/") or ".." in Path(x.filename).parts]
        assert not bad, f"unsafe archive path(s): {bad}"
        z.extractall(module)
    for name in ("customize.sh", "module.prop", "zygisk/arm64-v8a.so",
                 "webroot/index.html", "webroot/app.js", "webroot/style.css"):
        assert (module / name).is_file(), f"missing: {name}"
    for name in executables:
        target = module / name
        assert target.is_file(), f"missing runtime: {name}"
        assert target.stat().st_size > 0, f"empty runtime: {name}"
    # This is the critical scenario that ZIP validation never exercised.
    for item in module.rglob("*"):
        if item.is_file():
            item.chmod(0o644)
    assert not os.access(module / executables[0], os.X_OK), "precondition invalid"
    simulate = r'''
set -eu
set_perm() {
  [ "$#" -eq 4 ] || exit 61
  chmod "$4" "$1"
}
. "$MODPATH/customize.sh"
'''
    env = {**os.environ, "MODPATH": str(module)}
    p = subprocess.run(["sh", "-c", simulate], env=env,
                       text=True, capture_output=True)
    assert p.returncode == 0, (
        "installer customize.sh failed: " + p.stdout + "\n" + p.stderr)
    for name in executables:
        target = module / name
        assert target.stat().st_mode & 0o777 == 0o755, (
            f"missing installed +x: {name}")
    # Check native binaries are really Android ARM64 and not dummy scripts.
    elf = (module / "bin/fps-profilectl").read_bytes()
    zygisk = (module / "zygisk/arm64-v8a.so").read_bytes()
    assert elf[:4] == b"\x7fELF" and int.from_bytes(elf[18:20], "little") == 183
    assert zygisk[:4] == b"\x7fELF" and int.from_bytes(zygisk[18:20], "little") == 183
    assert b"/system/bin/linker64" in elf, "incorrect Android ELF interpreter"
    # On the host, replace ARM64 helper with a tiny stub to test that the
    # WebUI shell bridge finds the extracted, repaired bin/ directory.
    stub = module / "bin/fps-profilectl"
    stub.write_text("#!/bin/sh\n[ \"$1\" = read ] && echo '{\"custom\":{}}'\n")
    stub.chmod(0o755)
    p = subprocess.run(["sh", str(module / "bin/profile-ui.sh"), "read"],
                       capture_output=True, text=True)
    assert p.returncode == 0 and p.stdout.strip() == '{"custom":{}}', (
        f"WebUI bridge could not execute post-install helper: {p.stderr}")
    print("KernelSU simulated install PASS: chmod 0644 -> customize.sh 0755;",
          len(executables), "executables; Android arm64 ELF; WebUI bridge")
