# Batch 45 — the WordPress route tables of hosts the registry already held

**619 rows, every one measured end to end.** Probe, emit and **stored** all ran
for real, so each row in `docs/verified-sources-batch45.tsv` carries both numbers:
**245,583 emitted, 245,609 stored** over one pass of all 619 (stored exceeds
emitted by the per-run disclosure notices). Registry 21,789 → **22,408**.

| Beat | Rows | Hosts | What |
|---|---:|---:|---|
| `hp3b45_jpwpmedia.c` | 426 | 97 | Online media and magazines |
| `hp3b45_jpwpcorp.c` | 116 | 32 | Company sites |
| `hp3b45_jpwporg.c` | 24 | 8 | Associations and professional bodies |
| `hp3b45_jpwpculture.c` | 22 | 3 | Libraries and cultural institutions |
| `hp3b45_jpwpscience.c` | 11 | 1 | Research institutions |
| `hp3b45_jpwpcyber.c` | 9 | 3 | Security organisations |
| `hp3b45_jpwpenv.c` | 8 | 2 | Hazard and geospatial sites |
| `hp3b45_jpwpgov.c` | 3 | 1 | Government sites |

Record types: `site-post` 108, `site-page` 136, `site-user`
106, and 269 custom-post-type rows across
222 distinct `cpt-*` types.

## The method: the registry is its own host list

Batch 42 found that a Japanese WordPress host's own route table advertises far
more than `wp/v2/posts`. This batch applies the same question to a host list the
tree already vouches for: **every `.jp` host that a registered row already fetches**
(an RSS feed, an HTML register, a CSV) was asked whether it also runs WordPress
REST. Measured 2026-10-19:

| Step | Result |
|---|---:|
| `.jp` hosts in the registry with no WordPress row yet | 1,613 |
| Advertise a REST root | **245** (185 via the homepage's `Link: rel="https://api.w.org/"` header, 47 via the same relation as a `<link>` tag, 13 at a bare `/wp-json`) |
| … and answer `/wp/v2/types` | 149 |
| Collections asked for one page (`per_page=100`) | 901 |
| Answered 200 with ≥1 post- or user-shaped record | **691** |
| Dropped as editor/theme machinery (banners, sliders, forms, block patterns, popups, layouts) | 67 |
| Dropped as the same endpoint under a second hostname | 3 |
| Staged as manifest rows | 621 |
| `probe_hp_batch.py` | **619 PASS**, 2 failed (9 rows failed on connection resets on the first pass; one re-run recovered 7) |
| `audit_registry_emit.py --match '^JO45_'` | **581 OK**, 38 SLOW |

Other answers from the type lists: 121 collections advertised and empty,
31 answering something other than a list, 20 × 401, 9 × 403,
17 × 404, 10 transport failures — all kept in
`docs/rejected-sources-batch45.sweep.tsv`; probe failures in
`docs/rejected-sources-batch45.probe.tsv`.

**The REST root is advertised, not guessed.** A bare `/wp-json` answered on
13 hosts; the homepage's `Link` header named the root on 185, including
sites that serve WordPress from a subpath or a different hostname than the one
the registry knows. Asking the HTML page for its `rel="https://api.w.org/"`
relation is the step that found most of this batch.

## Beats are host TYPES, not collection types

Batch 42 split its tables by collection (`pages`, `users`, `cpt*`), which puts a
newspaper's author register and a bank's into one collector. Here the collector
is what the HOST is — media, company, association, library, security, research,
hazard, government — read off the category the registry already gives that host
(else its TLD), and each table holds every collection type its hosts publish;
`record_type` carries the distinction (`site-post`, `site-page`, `site-user`,
`cpt-<type>`). That is what lets `core/collectors.def` file each beat under one
group without a per-row override.

## What is NOT in this batch, and where it is

The same sweep was run over a second pool: **5,123 member-organisation hosts
harvested from nationwide federations' link pages** (JCCI's chamber list, the 47
JA prefecture pages, the 日本医師会 郡市区医師会 link database, 全国消防長会,
全社協, co-op maps, 税理士会 / 司法書士会 / 社労士会 / 行政書士会 lists, JC
地区協議会, 全国公立文化施設協会). 2,203 of them advertise a REST root and
**5,678 collections on 1,372 hosts answered with records**. They are NOT
registered: at ~5,700 scheduled rows on a 6-hour cadence that is a scheduler-load
decision, not a probe result. Every one is kept, with its count and
`X-WP-Total`, in `docs/staging-sources-batch45.federations.tsv`, so the next
round starts from the measurement rather than from the sweep.

```sh
cd native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch45.*.txt --jobs 8
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO45_' --jobs 10 --timeout 200 --out sweep45.tsv
```
