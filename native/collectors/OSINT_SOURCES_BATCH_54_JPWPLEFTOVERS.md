# Batch 54 — the content collections batch 49 left behind

32 WordPress REST collections on Japanese federation-member hosts, registered
2026-10-22. They are the part of batch 45's staging set that batch 49 did not
take, and the reason it did not take them is a defect in the sweep tool rather
than anything about the sources.

## How this batch came to exist

It started as an attempt to register the staging set, and the first measurement
stopped that: **batch 45's staging set is already registered.** Normalising
both sides (scheme and a leading `www.` dropped) and comparing endpoints,
**5,392 of its 5,643 are in the tree**, and 5,392 is exactly batch 49's own row
count — batch 49 *is* that staging set. The file was still headed "NOT
generated, NOT registered", so a session that believed its header would have
created 5,392 duplicate endpoints. That header is now a record of what is
registered, not a to-do list. CLAUDE.md records the same trap for batch 37,
whose file claimed "still unprobed" for a fortnight after it was probed.

The 251 endpoints genuinely absent were then read rather than counted:

| what | n | why |
| --- | --- | --- |
| theme and plugin machinery | 203 | `tools/wp_routes_sweep.py`'s `MACHINERY` regex rejects these by design — banners, sliders, carousels, block patterns, form definitions, template parts. A carousel's slides are not a record set |
| a domain takeover | 1 | `www.miyawakacci.or.jp`, whose posts collection samples as Polish casino spam ("Kasyno online BLIK…"). CLAUDE.md already records main removing that host from batch 49 |
| **real content wrongly rejected** | **48** | two over-broad alternatives in that same regex — below |

## The two over-broad rules, both found by measuring

* **`form` was unanchored**, so it matched in|**form**|ation, and silently
  rejected the whole `information` family: 30 collections of announcements,
  including `member-information` (**1,074 records** — Fukuyama chamber of
  commerce's member companies) and `members_information` (426). This is the
  substring trap CLAUDE.md already records for the takeover scan, where
  "cialis" is inside "Commercialisti" and "casino" inside "Liceo Pascasino".
* **`^xo_` was written for the XO Liteslider plugin** and also matched
  `xo_event`, the XO Event Calendar's post type — 8 collections of real events
  from medical associations and chambers of commerce, up to 573 records.

Anchored on a separator or a string edge, and with `^xo_` excluding `event`,
the two fixes **admit 39 of the 253 on 37 hosts and let no form or slider type
through** — measured over the whole leftover set, not argued.

`kubio` and `changeset` were added to the same regex afterwards, and that one
was caught by a GATE rather than by reading: three `kubio/*` rows shipped into
the generated C and `make lint-sources` answered **dup-endpoint**. kubio is a
page builder and those collections are its editor state; they had dodged every
anchored alternative because their `rest_base` contains a `/`. They are out,
with the reason in `docs/rejected-sources-batch54.tsv`.

## What shipped, and what the engine said

| beat | rows | hosts |
| --- | --- | --- |
| `hp3b54_jpwporg` | 15 | associations, co-ops, JA, medical and professional bodies |
| `hp3b54_jpwpmedia` | 15 | public facilities and local bodies |
| `hp3b54_jpwpcorp` | 2 | corporate sites |

Probe (`probe_hp_batch.py`, `docs/probe-sources-batch54.tsv`): **34 of 48
PASS**. One more was recovered by retry — `www.ja-sapporo.or.jp` answered 200
with 7 records when asked again, so its probe `NET_ERR` was this container's
egress — which made 35 generated, and the three kubio rows then left, leaving
32.

Emit (`audit_batch_emit.py` through the real binary,
`docs/emit-sources-batch54-2026-10-22.tsv`): **35 of 35 OK, 4,454 records
emitted** on one pass. That reading covers the three kubio rows too, because it
was taken before the lint removed them; nothing else in it changed.

## The 13 that did not ship, and the one verdict that is not about a source

`docs/rejected-sources-batch54.tsv` carries each with its reason:

* **2 `EMPTY_RESULTSET`** — a well-formed collection holding nothing, which
  would emit nothing on every run for ever. One is a `recruit_info` with no
  postings; the other is the kubio changeset.
* **8 `HTTP_ERR`** — three of them one host whose REST `types` endpoint answers
  **401** (a disabled REST API, which the earlier sweep saw as `types:401`),
  and one a 503 on two readings.
* **3 `LOCAL_EGRESS`, which is a statement about THIS CONTAINER and never about
  a host.** `www.toyama-familypark.jp`, `www.ib-ja.or.jp` and
  `www.yanagawa-cci.or.jp` answer status **000** on the probe, on a direct
  retry, **and on their host roots** — three unrelated hosts failing
  identically on a day this container has already produced egress resets
  elsewhere. They are recorded as unmeasured from here rather than dead, which
  is the distinction CLAUDE.md draws after a sweep attributed its own
  `ws_closed_mid_exchange` resets to 569 innocent upstreams.

A correction worth keeping about how that was measured: the first retry pass
reported `www.yanagawa-cci.or.jp` as returning 7 records, and it had not — the
script reused one `/tmp` output file between hosts, so a `status=000` left the
PREVIOUS host's body in place and the count was read off that. Per-host output
files gave the real answer, which is 000 for all three. A stale artefact from a
failed fetch reads exactly like a successful one.
