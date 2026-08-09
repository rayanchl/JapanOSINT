#!/usr/bin/env python3
"""Map each pure-probe source to its defining file and say whether the file is
safe to delete outright (i.e. it defines that source and nothing else).

Deleting a whole .c removes its REGISTER_SOURCE constructor, which is how a
source leaves the registry (the Makefile globs *.c). But several of these files
define more than one source, and a few sit next to real collectors — so the
per-file source count decides delete-file vs edit-in-place.
"""
import os, re, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")

# Pure reachability probes: no measurement, no credential path. From
# probe_census.py, minus the key-gated ones (those get fixed, not deleted) and
# minus the two that carry real payload (grid-usage-realtime, pref-police-crime).
REMOVE = """blitzortung-lightning comiket-events earthquake-network-citizen
fsa-crypto-exchanges gas-outages jcab-notams jcg-msi jftc-mergers jnto-arrivals
jpo-jplatpat jpo-trademarks jpx-quotes jr-boarding-stats jr-central-delay
jr-east-delay jr-west-delay kafun-pollen kakaku-prices mext-schools
mic-broadcast-towers mic-elections michi-no-eki mlit-road-traffic ndl-search
nexco-roadwork nhk-relay-towers nied-mowlas regional-grid-outages
saibansyo-rulings steam-jp-users sumo-tournaments teikoku-failures tepco-outage
tmp-protests tsr-closures xrain-radar yahoo-chiebukuro
boj-stats bosai-volcano-cam docomo-mobaku food-poisoning influenza-surveillance
jcg-navarea jma-pollen jma-uv mhlw-health ndb-open nict-atlas nitter-mirrors
ntt-fiber pogo-raids-jp tenki-jp tiktok-jp-discover vics-traffic vnet
wantedly-bizreach yahoo-auctions yahoo-crowd-map shinkansen-status""".split()

# Key-gated placeholder emitters: KEEP the source, stop it emitting a fake row.
FIX_GATED = """gitlab-bitbucket-leaks intelx-leaks luup-private psbdmp-pastes
securitytrails-history strava-segments-jp twitch-jp-streams vrchat-active-jp
whoisxml-reverse""".split()

idre = re.compile(r'\.id\s*=\s*"([^"]+)"')

files = {}
for fn in sorted(os.listdir(SRC)):
    if not fn.endswith((".c", ".inc")):
        continue
    try:
        t = open(os.path.join(SRC, fn), encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    ids = idre.findall(t)
    if ids:
        files[fn] = ids

owner = {}
for fn, ids in files.items():
    for i in ids:
        owner.setdefault(i, fn)

delete_whole, edit_inplace, notfound = [], [], []
for sid in REMOVE:
    fn = owner.get(sid)
    if not fn:
        notfound.append(sid)
        continue
    others = [i for i in files[fn] if i != sid]
    if not others:
        delete_whole.append((sid, fn))
    else:
        edit_inplace.append((sid, fn, others))

print("DELETE WHOLE FILE (%d):" % len(delete_whole))
for sid, fn in delete_whole:
    print("  %-28s %s" % (sid, fn))
print("\nEDIT IN PLACE — file has other sources (%d):" % len(edit_inplace))
for sid, fn, others in edit_inplace:
    print("  %-28s %-34s also: %s" % (sid, fn, ",".join(others[:4])))
print("\nNOT FOUND (%d): %s" % (len(notfound), " ".join(notfound)))
print("\nGATED, keep+fix (%d):" % len(FIX_GATED))
for sid in FIX_GATED:
    print("  %-28s %s" % (sid, owner.get(sid, "?")))

json.dump({"delete_whole": delete_whole, "edit_inplace": edit_inplace,
           "notfound": notfound},
          open(os.path.join(HERE, "probe_removal_plan.json"), "w"), indent=1)
