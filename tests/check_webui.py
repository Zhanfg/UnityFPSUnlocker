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
assert "dumpsys package" in bridge
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
