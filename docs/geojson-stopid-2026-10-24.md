# The geojson `stop_id` precedence defect — measured, patched, not yet built

2026-10-24. Status: **patch written and anchor-verified, NOT applied** — the
WSL build host was mid-disk-migration, so nothing here has been compiled or
gated. Apply with the script named at the bottom, then run the gates.

## The defect

`lib/geojson.c`'s `feature_native_id()` resolves a Feature's identity by
scanning `NATIVE_ID_KEYS` over `properties` **first**, and only then falling
back to the Feature's own top-level `id`. `stop_id` sat in that list, ahead of
the feature id.

A GTFS `stop_id` is unique per **agency**, not globally. So for any layer that
pools several agencies, the uid keyed on a GROUP rather than a record — which
is the same class of mistake rule 4b names, and which `geojson.c`'s own
collision-guard comment already describes for `station_id`: "names like
`station_id` and `id` that are routinely a DIMENSION rather than a record
identity".

## The measurement (not new — it was sitting in a collector header)

`collectors/feed/generated/vsrc13_us_state_1.c`, measured live 2026-09-08
against `nam-ags-california-transit-stops` (88,247 features):

| reading | value |
| --- | --- |
| features sampled | 10,000 (20 pages of 500) |
| distinct `stop_id` | 9,084 — `"105"` appears 8×, `"154"` 7× |
| distinct layer `id` (ArcGIS OBJECTID, `f=geojson`) | 10,000 |
| collisions | 916 |
| rescued by the in-page guard | 111 |
| **LOST per pass** | **805** |

The in-page guard (`gj_collision_map`) only ever sees one 500-feature page, and
colliding agencies mostly land in *different* pages — which is why 805 of the
916 were lost rather than content-hash-disambiguated. That row's own header
states the general repair "belongs in lib/geojson.c (a properties key that is
only unique within a group must not outrank the feature's own id)".

Note the row itself no longer waits for this: it carries
`VGEO_IDKEYS(..., "OBJECTID")`. The fix matters for every *other* multi-agency
GeoJSON layer that has not been hand-repaired.

## Why only `stop_id` moves

Demoting the whole group-ish class would override deliberate decisions:

* **`station_id` — do not demote (45 sites).** `k_net.c` and
  `national_parks.c` each carry an AUDIT NOTE recording that they *replaced* a
  positional id (`KNET_LIVE_<i+1>`, `PARK_LIVE_<i+1>`) with a **stable OSM
  node/relation id** written into exactly this field, because the positional
  one re-identified every row whenever the upstream reordered. Demoting the
  field would discard that repair. These want measuring one source at a time.
* **`buoy_id`, `spring_id`** — one site each, same reasoning, no measurement.

And demoting `stop_id` is **provably a no-op for every collector of ours that
synthesizes it**. Five do — `gtfs_jp.c`, `osm_transport_buses.c`,
`trn_hongkong_citybus_network.c`, `trn_bvg_berlin_stops_nearby.c`,
`trn_mbta_gtfs_static.c` — and none sets a top-level feature `id`, so
`feature_native_id()` finds no `feat.id`, falls through to the group class, and
still keys on `stop_id`. Nothing re-keys. The change bites only where the
*upstream* GeoJSON supplies both, which is the broken case.

## A route that does NOT work, and why

Disambiguating at the collision guard instead — "when a key collided in this
page and the feature has its own id, use the id rather than the content hash" —
is strictly safer (it re-keys nothing) but **does not fix this**: only 111 of
the 916 collisions were visible in-page. The 805 lost ones collided across
pages, so the key has to be globally unique *before* the walk, not repaired
after it.

## What is still unverified

* Not compiled. `-Wall -Wextra` at 0 warnings is a requirement, not an
  assumption.
* `make hptest`, `make unit`, `make audit-sources` have not run.
* **Re-keying:** rows already stored under a `stop_id`-derived uid for an
  upstream layer that also publishes `id` will get new uids, orphaning the old
  rows. `tools/cleanup_stale_uids.py` is the tool for that class (it reports by
  default and only deletes rows whose replacement is proven to exist). Run it
  in report mode after the first pass and read it before `--delete`.
* No re-measurement against the live layer. The 805 is a 2026-09-08 reading.

## Apply

    python3 <scratchpad>/patch_geojson_stopid.py     # from the repo root
    cd native && make && make hptest && make unit && make audit-sources

The script refuses to apply unless all three anchors match exactly once, so a
file that moved underneath it fails loudly instead of corrupting.
