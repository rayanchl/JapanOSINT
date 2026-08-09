import json, subprocess
out = subprocess.run(["curl", "-sS", "-m", "25", "-A", "JapanOSINT/1.0 (osint@example.org)",
                      "https://api.adsb.lol/v2/point/35.68/139.76/100"],
                     capture_output=True).stdout
try:
    d = json.loads(out)
except Exception:
    print("parse fail:", out[:300]); raise SystemExit
ac = d.get("ac", [])
print("live aircraft near Tokyo:", len(ac))
for a in ac[:8]:
    print(" hex=%s reg=%s flight=%s type=%s lat=%s lon=%s"
          % (a.get("hex"), a.get("r"), (a.get("flight") or "").strip(), a.get("t"),
             a.get("lat"), a.get("lon")))
