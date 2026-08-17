# Penetrancy pass, 2026-08-17 — the hops we already owned

## What was asked, and what this session could actually verify

The ask was batch 18: find high-OSINT-value sources worldwide, add them, test
them. **No source was added, because no source could be tested.** This session's
egress policy allows GitHub and the language package registries and nothing
else. Both routes out were tried and both are blocked at the same proxy:

```
$ curl -sS -o /dev/null -w "%{http_code}" https://api.gleif.org/api/v1/lei-records
000     # CONNECT tunnel failed, response 403

WebFetch https://api.reliefweb.int/v1/disasters?limit=2
{"error_type":"EGRESS_BLOCKED","domain":"api.reliefweb.int"}
```

`$HTTPS_PROXY/__agentproxy/status` records each denial as
`gateway answered 403 to CONNECT (policy denial or upstream failure)`.
Web *search* works, because it runs server-side — which is exactly the trap. It
returns documentation, and documentation is where dead endpoints come from. The
discovery brief in `.claude/workflows/osint-batch17-global.js` opens with the
rule for a reason:

> RULE 1 — NEVER return an endpoint you did not personally fetch and read the
> body of. Not one from documentation, not one whose sibling worked.

Batch 16 measured what that rule is worth: of 1,196 endpoints agents claimed to
have fetched, 741 survived an independent re-probe and **413 actually emitted a
record**. A batch assembled from search results instead of responses would have
been mostly fiction, and it would have been fiction wearing the word "verified".
So the session spent itself on the highest-value work that needs no egress —
and there was a great deal of it, because the tree was already holding hops it
had paid for and never walked.

## 1. 362 proven detail hops, finally walked

Batches 16 and 17 did two separate things. They verified ~840 list endpoints
live, and they probed a **per-record detail endpoint** for many of them with a
real id and watched a record come back. The list halves shipped as one-GET
`VJSON` collectors. The proven detail halves went into
`docs/candidate-sources-batch{16,17}.detail-hops.tsv` and were never wired.

382 collectors went out describing the situation themselves:

> "A per-record detail endpoint was verified for this source; see the batch's
> detail-hops side-car. **This collector fetches the list endpoint only.**"

That is rule 2 violated in writing. The request is spent, the route to the
record behind each hit is known *and proven*, and it is dropped —
while `hp_source.detail_url` / `detail_key` has been sitting in
`lib/hpengine.h` the whole time.

362 rows now walk it, in 128 generated tables
(`native/collectors/feed/generated/hp1[67]_*.c`). What that changes for an
investigator:

| source family | was | now |
|---|---|---|
| IHSN / World Bank / national NADA microdata | study title + access class | full DDI: producers, contact persons with email, sampling, version statement |
| GLEIF LEI records | legal name + country | whole LEI record incl. registeredAs company number, parent/child ownership links |
| d-portal (IATI) aid activities | activity title + commitment | the per-activity **transaction ledger** — values, currencies, flow/finance/sector codes |
| UNDP transparency | project title | the project record |
| openFDA device/drug identifiers | one frozen example record | any k-number, PMA, UDI key, NDC, application or recall number |

Records past the per-run budget (`$JO_HP_DETAIL_MAX`) are stamped
`_detail_pending`, so an un-fetched detail is never mistaken for an absent one.

**Not converted, and reported rather than fudged:**

* 8 `VRSS`/`VGEO` rows — hpengine has no RSS or FeatureCollection mode, so they
  stay list-only instead of being half-moved.
* 12 rows in the new `docs/detail-hops-unwired.tsv`. Eleven have a proven
  `detail_url` with a `{v}` slot but the side-car never recorded **which list
  field fills it**; one hard-codes an id into the URL. The missing piece is one
  probe each. Inventing a key name would be guessing at other people's schemas.

**The generator reads the `.c` files, not the manifest TSV** — deliberately.
Batch 16 lost sources to exactly that shortcut: regenerating from candidate rows
put the discovering agent's *predicted* array path back over the verifier's
*observed* one, and the collectors emitted nothing. The `.c` file is the artefact
that was measured emitting records, so it is the authority.

## 2. One page walk, shared by both engines

Wiring a detail hop means moving a row from `VJSON` onto hpengine — and that
quietly cost it every page after the first, because only `jsonlist` had an
auto-pager. Buying penetrancy with discarded data is still discarding data, so
the logic moved to `lib/pager.{c,h}` and both engines call it.

It advances **only on the upstream's own evidence**, in this order:

1. a next link the server published (`links.next`, `@odata.nextLink`, …);
2. a cursor whose page-size sibling is in the URL and whose page came back
   exactly full — a short page means the upstream is finished;
3. a page size the URL spells its own way (`rp`, `itemsPerPage`, `num`), which
   is **proven by the response**: when some numeric parameter equals the number
   of records the page returned, that parameter is the page size and the server
   just demonstrated it. `?page=1&rp=10` answering with 10 records has said what
   `rp` means;
4. failing all of that, its **declared total**. `?page=1` with no page size
   anywhere (ROR, bio.tools) is not an unpaged endpoint — it is a paged one whose
   page size only the server knows. If it says it has 100,000 and has handed over
   20, it has said itself that more remains.

Only page-*number* cursors advance on rule 4: an offset cursor needs a stride,
and inventing one from a running total is the invention this file refuses to
make. NADA/IHSN's `ps` is deliberately **not** in the cursor table — its cursor
is probably `page`, but a host that declares `ps` and ignores `page` would hand
back page 1 for every step of the walk, and "probably" is the wrong standard for
a table whose whole job is to advance only on evidence.

Two honesty bugs fell out:

* **`page_param` appended instead of set.** The walk built
  `…&page=2&page=3&page=4` and left the winner to the server; one that takes the
  first re-read page 2 until the ceiling.
* **A ceiling stop reported only what it had counted**, which reads as "nothing
  was missed" precisely when something was. It now reports the upstream's own
  declared total, labelled with where the number came from.

## 3. The audit was crying wolf, and hiding real findings behind the noise

`make audit-sources` at HEAD: 168 findings across 99 files. Now **118 across
76** — and that is after ADDING a check that surfaced 12 findings nobody could
see before (§5). The strict gated set grew from 30 files to 159.

Two thirds of the `single-page` findings were never real: `?page=1&per_page=100`
**is** walked by `jsonlist_emit_paged`. The audit did not model the engine, so it
reported 21 engine-walked rows as discards — and an audit that cries wolf on 21
rows is one whose other 18 lines nobody reads. It now checks both halves: is this
row on a paging engine, and can the pager actually move this URL.

Also fixed in the tooling itself:

* `--strict` took one glob and **silently kept the last** when passed two —
  the failure mode where a gate prints "0 findings" for a set it never scanned.
* `tools/lint_sources.py` counted **no hp rows at all** (they register through
  `hp_register()`, not `REGISTER_SOURCE`), so 30 shipped tables were invisible
  and `make source-floor` could not see an hp row vanish. True count: 11,170, not
  10,564. Parsing hp tables needs string-awareness — the braces inside
  `"…/catalog/{v}"` split rows in the wrong places otherwise.
* `dup-endpoint` counted a shared `{v}` detail route as duplicated work. 24
  country rows sharing `.../catalog/{v}` fetch 24 **disjoint** record sets. `{q}`
  search URLs still count, since two rows sharing one really would answer the
  same query twice.

## 4. Real discards, fixed

**`limit-one`: 11 → 0.** Nine were openFDA probe URLs shipped as scheduled
collectors with the example id frozen in — `search=k_number:"K092877"&limit=1`,
re-fetching one 2009 device clearance every twelve hours, on rows whose own
descriptions called them detail hops. They are pivots now; `limit=100` and
openFDA's `skip`/`limit` contract come from four rows `hp2_northam_us_reg.c` has
already shipped. The other two asked for **one** record out of NYC's whole
Socrata catalogue and out of an 8,858-document FDA archive.

**`dedupe-ring`: 1 → 0.** HUDOC's `char *seen[128]` and gov_money's
`char *seen[64]` both stopped *recording* at the ceiling while their loops kept
emitting, so entries past it could be emitted twice. `lib/seenset.h` grows and
has existed for this.

**Four bespoke collectors now walk instead of peeking:**

| collector | was | now |
|---|---|---|
| GovInfo USCOURTS | 100 of the 45,105 packages its own header documents | follows the server's `nextPage` under a budget keyed to whether a real `GOVINFO_API_KEY` is present — DEMO_KEY's ~30 req/hour is shared, so an unbounded walk would spend the next collector's allowance too |
| ECHR HUDOC | newest 50 of 89,604 judgments | walks `start`/`length` newest-first, dedupe across the whole walk |
| dane.gov.pl | 20 datasets, page pinned at 1 | follows JSON:API `links.next` |
| Rocky Linux Apollo errata | one page of 100 | walks the 1-based `page` cursor |
| AlmaLinux errata | **row 801 onward dropped from a 25 MB document already parsed in memory** | cap removed; every in-window erratum emitted, and the window itself disclosed |

`lib/truncnotice.{c,h}` is new. The same twenty-line disclosure had been
hand-copied into five collectors and had already drifted — some name a remedy,
some forget the upstream's count — so the shape a consumer must recognise
depended on which collector produced it.

## 5. A cap class nobody could see

Reading the OFAC consolidated sanctions parser turned up a bound the audit had no
pattern for:

```c
while (cJSON_GetArraySize(akas) < 24 && sanc_xml_next(&c2, al.body_end, "aka", &ae)) {
```

Twenty-four aliases, then stop. OFAC publishes designations with more, and on a
**sanctions list an alias is the thing screening matches on** — so alias 25
onwards was a false negative on a designated person, produced silently. Neither
existing check could see it: `record-cap` wants a `#define …MAX`, `loop-break`
wants a `break`, and this was a bound in the loop's own condition.

The new `loop-cap` check covers that shape, and only for counter-ish names, so a
`chars < 280` guarding a UTF-8 buffer is not swept in. It immediately surfaced 12
more. Fixed here, all on documents already fetched and parsed:

| collector | was |
|---|---|
| OFAC consolidated | aliases past the 24th dropped |
| TDnet timely disclosure | **100 rows per day** — a busy day at the Tokyo exchange runs past that, so it truncated exactly when it mattered |
| EDINET-x (`corp_financials`) | 25 filings per fetch |
| PR TIMES (`corp_markets_media`) | 30 press releases per fetch |

Nine remain, all camera-aggregator scrapers (`cam_*.c`) with per-run scrape
budgets of 60–300 anchors across paginated listings. Those budgets are arguably
politeness rather than accident, but none of them discloses when it bites, which
is what rule 6 requires. They are left visible as findings rather than
half-fixed: each has a different loop shape, and a bound that a reader can see is
better than eight rushed edits.

Alongside them, 5 `first-only` sites were confirmed as genuine fixed-shape tuple
reads (RDAP jCard's `[name,params,type,value]`, RFC 7484's `[[tlds],[urls]]`,
`[lon,lat]`, a `[from,to]` interval) and marked, and 2 as display picks whose full
array is already in `properties` (MISP galaxy `references`, UK sanctions
aliases). UK flood stations now carry `label_all` for multi-named stations, and
NASA POWER joins its whole `sources` provenance list instead of naming the first
of several and misattributing the series.

## What is left, honestly

* **118 audit findings** across 76 files: 40 `first-only`, 32 `record-cap`, 26
  `loop-break`, 11 `single-page`, 9 `loop-cap`. Most `record-cap`s look like
  memory guards rather than discards, but each needs a human read; that is why
  the number is published rather than baselined away. The 9 `loop-cap`s are the
  camera scrapers described above and are the most likely to be real.
* **The 11 remaining `single-page` rows** are `?page=1` with no page size in the
  URL. The engine now walks them *if* the upstream declares a total, and the
  audit cannot know statically whether it does — so they stay flagged. Reading
  one live response each settles them.
* **12 unwired detail hops** (`docs/detail-hops-unwired.tsv`) need one probe
  each to learn which list field feeds the `{v}`.
* **`esma-solr-sanctions`' page size was raised from `rows=1` to `rows=100`** on
  Solr's own contract, and **not re-probed** — flagged in
  `collectors/gen_detail_hop_upgrade.py`. If ESMA refuses it, the row reports a
  fetch failure, which is the correct degradation.
* **`eur-api-dane-pl`** is now strictly dominated by the bespoke dane.gov.pl
  collector — same endpoint, same array, same cadence, but page 1 only and
  without the JSON:API relationships. Dropping it is an inventory decision, not
  a refactor, so it is written down rather than taken unilaterally.
* **Batch 18 itself.** The discovery brief and schema are ready in
  `.claude/workflows/osint-batch17-global.js`; it needs a session whose egress
  policy permits the hosts being probed.

## Verification

```
make                 clean (-Wall -Wextra)
make hptest          63 assertions, all passing (14 new)
make audit-sources   tree-wide 168 findings across 99 files -> 118 across 76
make audit-sources   strict set: 159 files, 0 findings
make source-floor    11,170 >= 11,170
tools/lint_sources.py  OK — dup-id 0, dup-endpoint at baseline
--selftest, unit     PASS
```

Every source id registered before this work is still registered: the full id-set
diff across the detail-hop conversion is **0 removed**, which is the check that
matters when 362 rows change which macro registers them.
