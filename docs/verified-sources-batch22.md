# Batch 22 — 350 rows across four threat-infrastructure beats

Every row below was fetched over the wire on this machine, parsed in its
declared mode, run through the **real binary** and read back as
`emitted N of M available`. Nothing here was authored from documentation.

Manifests (the source of truth) —
`docs/candidate-sources-batch22.{breachcat,extortion,onion,pastechat}.txt`.
Generated tables — `native/collectors/pivot/table/hp3b22_*.c`. Edit the
manifest. Rejects are kept as data in `docs/rejected-sources-batch22.tsv`
(31 rows) and `docs/rejected-sources-batch22-onion.tsv` (25 rows, the onion
beat's own discovery-time rejects).

Scope rule for the whole batch, stated in every manifest header: **metadata
and infrastructure only**. Catalogue, event and decision records — which
organisation, which group, when, what data class, that a channel/instance/
relay exists and how active it is. Never a leaked payload, never credential
content, never a per-person or per-account lookup path.

## What shipped

| beat | rows | what it is |
| --- | --- | --- |
| `pastechat` | 148 | directories of fringe and federated platforms — Lemmy community lists, PeerTube channel lists, Mastodon instance activity, Discourse category trees, Matrix public-room directories, Nostr relay NIP-11 documents, nodeinfo — plus seven **entity pivots** that describe an instance by hostname (`NODEINFO_*_PIVOT`, `MATRIX_WELLKNOWN_CLIENT_PIVOT`, `DISCOURSE_BASICINFO_PIVOT`, `ATPROTO_DESCRIBE_SERVER_PIVOT`, `NOSTR_NIP11_PIVOT`) and one that describes a breach by name (`HIBP_BREACH_BY_NAME`) |
| `extortion` | 100 | ransomware victim / data-leak-site trackers (ransomware.live posts — 31,172 records in one fetch — ransomlook groups and markets), abuse.ch JA3 fingerprints, and the threat-actor / TTP / campaign registries: 94 MISP galaxy siblings, MITRE ATT&CK, DISARM, APTnotes |
| `breachcat` | 91 | breach and exposure **catalogues**: Washington State AG breach notifications (7,272 records), the HIBP breach catalogue, and 87 GDPRhub DPA-decision and court indexes by jurisdiction |
| `onion` | 11 | Tor network measurement — the metrics.torproject.org CSV series (bandwidth, network size, relays/bridges by IPv6, platforms, dirbytes, onion services seen, rendezvous cells, web stats) and the CollecTor index. onionoo, ahmia, exit-addresses and the Tor blog were already registered and are not reused |

`lint-sources` counts `REGISTER_SOURCE` only, so these 350 `HP_REGISTER_TABLE`
rows do not move the number it prints. The built binary's own seed count is the
real one: 13,169 → 13,519.

## Gate results

| gate | result |
| --- | --- |
| `gen_hp_batch.py` | 350 rows, four tables, no duplicate opts, no swallowed `\;` |
| `probe_hp_batch.py` | **350 of 350 PASS.** 44 GDPRhub rows 503'd under `--jobs 8` and every one passed on a serial retry — gdprhub.eu rate-limits, it is not dead |
| `probe_hp_batch.py --check-filter` | **7 of 7 pivot rows PASS** — each honours its filter (an impossible entity returns nothing or something much smaller) |
| `batch_exclusions.py --bin` | 0 collisions against the 13,169-id registry after two in-batch fixes (below) |
| `audit_batch_pagination.py` | 0 of 350 flagged; 94 declare `pagination_ok` |
| `audit_batch_reachable.py` | 0 of 350 can never run |
| `audit_batch_emit.py --jobs 6 --timeout 180` | **350 of 350 OK, 0 `DROPS_EVERYTHING`, 0 `SLOW`. 132,623 records emitted** — breachcat 13,311 · extortion 57,615 · onion 51,869 · pastechat 9,828 |
| `make` | 0 warnings |
| `make selftest / unit / hptest / lint-sources / audit-sources` | all pass; audit 0 findings, strict set 0 |

## Rows removed before shipping

| id | why |
| --- | --- |
| `RANSOMWARE_LIVE_POSTS` (onion) | same endpoint as `RANSOMWARELIVE_POSTS` (extortion), written by two source agents independently. `batch_exclusions.py` caught it |
| `MATRIX_PUBLICROOMS_FENEAS_ORG` | host no longer resolves |
| 27 federated-platform rows | dead, 403, 401, 404 or TLS-broken hosts — Lemmy (5), PeerTube (3), Discourse (3), Matrix (7), Nostr relays (9). Each failed the probe twice (parallel and serial) and the engine the same way. See the TSV for the per-row error |
| `NOSTR_RELAY_{ATLAS,EDEN,NOSTR}_LAND` | three subdomains of one operator returning the identical NIP-11 info document — a single object, not a record set (`EMPTY_RESULTSET`), three times |
| `MASTODON_ACTIVITY_DET_SOCIAL` | answers the probe but returns **429 to the engine on every attempt**, including a serial single-request rerun. A source that always 429s is an `EMPTY_RESULTSET` with extra steps |

## Also in this pass — repo salvage after the 2026-08-24 multi-agent audit

The audit session that authored this batch was cut off by an account session
limit with twelve agents mid-flight. What it left in the worktree built clean
(0 warnings) but had one gate regression and one unfinished batch:

* **`lint-sources` `dup-endpoint` 484 → 485.** Agent B removed the `?limit=100`
  record cap from `eur-sejm-votings` (a real exhaustiveness fix) — which made
  it byte-identical to `cyb-sejm-pl-prints`, already registered. The row was
  mislabelled anyway (named "votings", fetching prints). Removed; the
  transparency row stays.
* **Batch 22 manifests were committed-in-spirit but their `hp3b22_*.c` tables
  were never generated into the repo** — the source agents built in their own
  WSL trees. Regenerated from the manifests and run through every gate above.
* **`/api/layers` v2 (agent M1's map taxonomy — `core/layers.def`,
  `core/layertab.c`, `core/dataapi.c`) verified live**: 117 layers, modality
  declared not inferred (23 point · 1 heatmap · 2 polygon · 1 line · 1 raster ·
  89 `null` = undecided), crime split into distinct `police-crime-points` /
  `police-crime-heatmap` layers that share no sources, and
  `/api/layers/:id/geojson` now a real FeatureCollection with in-band
  `records_available` / `records_used` / `truncated` / `next_offset` meta in
  place of the old permanent-`[]` stub. The first check reported "0 layers"
  only because the test server had no `SUPABASE_JWT_SECRET` and answered
  `503 Auth not configured` — a test-harness fault, not a server one.
