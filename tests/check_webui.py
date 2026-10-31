#!/usr/bin/env python3
from html.parser import HTMLParser
from pathlib import Path
import re

class IdParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
    def handle_starttag(self, tag, attrs):
        for k, v in attrs:
            if k == "id":
                assert v not in self.ids, f"duplicate HTML id {v}"
                self.ids.add(v)

root = Path(__file__).resolve().parents[1]
html = (root / "webroot/index.html").read_text()
js = (root / "webroot/app.js").read_text()
css = (root / "webroot/style.css").read_text()
bridge = (root / "packaging/profile-ui.sh").read_text()
service = (root / "packaging/service.sh").read_text()
guard = (root / "packaging/perf-guard.sh").read_text()
m = IdParser()
m.feed(html)
refs = set(re.findall(r'\$\("([A-Za-z][A-Za-z0-9]*)"\)', js))
missing = refs - m.ids
assert not missing, f"JS references missing controls: {missing}"
for id in ("rescan","search","apps","save","remove","cpu","gpu","fps","interval",
           "delay","restore","manualadd","manualpkg"):
    assert id in m.ids
assert "exec(" in js and "getPackagesInfo" in js
assert "validPkg" in js and "shellEscape" in js
assert "set " in js and "remove " in js
assert "cpu_min_khz" in js and "gpu_min_mhz" in js
assert "scan)" in bridge and "caps)" in bridge and "restore)" in bridge
assert "pm list packages -3" in bridge
assert "cmd package list packages -3" in bridge
assert "pidof" in service
assert "scaling_available_frequencies" in guard
assert "available_frequencies" in guard
assert "safe_floor" in guard and "restore()" in guard
assert len(css) > 1000
assert "http://" not in html and "https://" not in html, "WebUI must work offline"
print(f"WebUI integrity PASS: {len(m.ids)} unique controls, {len(refs)} JS references")

# OPGCE bridge must never mutate the vendor COSA database or bundled binary.
opgce = (root / "packaging/opgce-bridge.sh").read_text()
for field in ("opgceStatus", "opgceProfile"):
    assert field in m.ids
for fragment in ("opgce-status", "opgce-list", "state.opgce.catalog", "OPGCE"):
    assert fragment in js or fragment in bridge
assert 'managed)' in opgce
assert 'enabled()' in opgce
assert 'ACTIVE_PROFILES|' in opgce
assert 'sh "$OPGCE" managed "$1"' in service
assert 'sh "$GUARD" restore' in service

scrc = (root / "packaging/scrc-bridge.sh").read_text()
for element_id in ("scrcStatus","scrcProfile","scrcWarning"):
    assert element_id in m.ids
for token in ('scrc-status','scrc-list','state.scrc.catalog','DUAL_INJECTORS'):
    assert token in js or token in bridge or token in scrc
assert 'sh "$SCRC" active' in service, "SCRC must own scheduling before local frequency floors"
assert 'SCRC\\ *)' in service, "SCRC handling must trigger restore"
assert 'encrypted_oplus-config' in scrc
assert 'managed)' in scrc and 'active)' in scrc

# The WebUI must execute slow game scan and FPS evidence collection
# asynchronously, while per-game settings remain fresh for each app launch.
for element_id in ("measure", "evidence", "diagnose", "diagOutput"):
    assert element_id in m.ids
assert 'nativePackages()' in js
assert 'sukisuPm()' in js\nassert 'sukisu.listPackages("user")' in js\nassert 'api.listPackages("user")' in js\nassert 'pm.getPackagesInfo(arg)' in js\nassert 'listUserPackages' in js and 'listPackages' in js\nassert 'ksu://icon/' in js\nassert 'ZYGISK_PROVIDER_CANDIDATES' in (root / 'packaging/game-diag.sh').read_text()
assert 'root("measure " + shellEscape(pkg), true)' in js
assert 'window[callback] = finish;' in js
assert 'api.exec(command, callback)' in js
assert 'measure)' in bridge
assert 'fps-evidence.sh' in bridge
assert 'game-diag.sh' in bridge and 'diagnose)' in bridge
assert 'committed.fps!==fps' in js
main = (root / "UnityFPSUnlocker/main.cc").read_text()
assert "std::lock_guard<std::mutex> lock(config_mutex);" in main
assert "const auto result = LoadConfig(); !result.ok()" in main
assert "file_watch_listener" not in main
