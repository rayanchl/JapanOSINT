# Source repair, round 7 — 2026-09-11

*Three agents, disjoint files, 213 ids from the remaining-defect list; every id
reached. Continues the rounds recorded in `docs/systemwide-audit-2026-09-04.md`
(§3–§5), which left 989 sources non-OK out of the original 1,770.*

Method unchanged and non-negotiable: a verdict is a live fetch plus two counts
(records on the page vs distinct values of the declared key). An agent's own
count is never the evidence — the re-sweep against the rebuilt binary is.

---

## 1. Before touching anything: the work list was partly stale

Two of the three agents independently found that rows in the sweep TSV carried
pre-fix numbers, because that sweep had been produced with `--resume` and
because `hp3b19_latam.c` already held an uncommitted repair pass from
2026-09-07. `BR_SICONFI_RREO`'s listed 5065/621 is verbatim the number written
in that file's own comment as the defect it had already fixed.

Both re-measured every id against the current binary before editing. Seven
latam rows were already correct; two were **worse** than the list said
(`CO_UPME_PRECIOS_MINERALES` 5,868 → 5,559 stored, listed as 5,824), and
`IT_FVG_MOBILE_SITES`, `IANA_LANGUAGE_SUBTAGS`, `AM_OPENDATA_BULK`, the
`SI_GURS_*` and `HU_INSPIRE_*` rows were all already repaired.

That is the reusable lesson, not a footnote: **a sweep TSV is a snapshot, and
`--resume` makes it a snapshot of several different moments.** Re-measure the
row before you fix it.

---

## 2. What was repaired, by cause

**Wrong identity declared (the rule-4b class).** `ANFR_HANDSET_SAR` keyed on
`ref_dossier`, but a dossier covers up to four handsets — `fields._id` gives
1,230 of 1,230. `BR_BCB_EXPECTATIVAS` had no `id_keys` at all and titled
records with the forecast *mean*, so forecasts keyed on their own average;
`Indicador+IndicadorDetalhe+Data+DataReferencia+baseCalculo` gives 1,000 of
1,000. Sixteen ChEMBL rows carried no field in jsonlist's fixed id precedence
list and fell back to a hash of (title, link, date) where the title is a name
that repeats — every one converted to `VJSON_KEYED` on the upstream's own id
(`site_id`, `activity_id`, `molecule_chembl_id`, …), 2,000 of 2,000 distinct in
each case. Three Socrata rows moved to Socrata's own `:id` with
`$select=*,:id&$order=:id`.

**Paging that silently re-served page 1.** The 31 `us-socrata-metadata-*` rows
declared `limit=100` + `offset=100`, and that endpoint *ignores* `offset` while
honouring `page` — 200 emitted, 100 stored, forever. Eleven `us-arcgishub-*`
rows walked `startindex` over an unstable relevance order: 2,000 emitted,
1,571 distinct, fixed to 2,000/2,000 with `&sortBy=properties.created`
(`sortBy=id` and `created` without the `properties.` prefix are both 400).
`BR_PNCP_CONTRATOS` is the engine bug in §3.

**Not a record source in the shape declared.** `PR_PRSN_EARTHQUAKES` declared
`HP_HTML` against RSS 2.0 XML. `CZ_NKOD_SPARQL` was 406 on the engine's
`Accept: application/json` and 200 on `application/sparql-results+json`; its
`LIMIT 50` also became `LIMIT 100000` — **50 → 53,382 records**, and its key
became `d.value+t.value` because keying on the IRI alone would drop every
second-language title. Six NASA Exoplanet Archive rows were losing records to a
too-narrow `select` (stellarhosts 40,982 → 44,904 distinct; the `select *`
ceiling is 44,929 but costs 157 MB and 108 s, which the macro's 25 s timeout
cannot hold — the rows now carry the widest projection that fits and say so).
`ca-gov-news` returned zero entries because the API's default page size is 0;
`&pick=500` returns 500.

**Too slow for the default timeout.** `BR_ANAC_AERONAVES` (23 MB, 79.6 s),
`CL_SNIFA_UNIDADES` (39.9 MB, 62.6 s), `CR_SNIT_DISTRITOS` (21.9 MB, 38.8 s)
and `BR_PNCP_ATAS` (26–36 s, once a 504 at 70 s) were all aborting at the 40 s
default. Each now declares a `timeout_ms` sized to its measured response.

**Correct dedupe, not a defect (case B) — recorded with its evidence.**
`gr-diavgeia-positions`: 25,188 records, 233 distinct (uid,label) pairs, one
repeated 2,989× byte-identical. `BR_IBAMA_TERMO_EMBARGO_ITENS`: 48,776 rows,
48,591 distinct *full* rows — IBAMA publishes 185 byte-identical lines.
`DK_MASTEDB_ANTENNAS`: 69,699 records, 40 exact repeats. `MCC_MNC_TABLE`: 41.
Two federated Socrata searches hand the same asset back twice because
`order=createdAt` has ties and the offset window straddles them. Each row now
carries the numbers, so nobody "fixes" them later.

**Diagnosed and left honestly failing (case C).** 37 CelesTrak rows —
`celestrak.org` never completes a TCP handshake on :443 or :80, from WSL and
from Windows, retested over 40 minutes. Both Jamaica portals reset mid-TLS.
Two PanamaCompra endpoints send a connection-specific `keep-alive` header over
HTTP/2, which RFC 7540 §8.1.2.2 forbids and nghttp2 aborts on (curl 92) —
`--http1.1` returns the real JSON, and the ABI has no HTTP-version knob. About
twenty government news feeds are 403/404/405 bot walls or retired feeds whose
replacement does not exist; each was re-checked against the site's current
feed index before being left alone. None of these were papered over.

---

## 3. Engine defects found through the round, and fixed centrally

**`page_param` appended instead of replacing** (`lib/hpengine.c`). The next
page was `snprintf("%s&%s=%ld", url, param, n)`, so a row whose URL already
bound its page parameter — required by some upstreams; PNCP answers 400 without
`pagina` — asked for `…&pagina=1&pagina=2`. Spring and JAX-RS bind the *first*
occurrence, so the walk re-read page 1 to the ceiling: N pages emitted, one
page stored, `rc=0`. `BR_PNCP_CONTRATOS` lost 4,499 of 5,000 records this way
with every gate in this repository green. Now replaced in place on a real
parameter boundary (`?key=` / `&key=`, so paging on `page` does not rewrite
`page_size`), pinned by `tests/hpengine_test.c` "9f-bis" — whose fixtures fail
if the parameter is ever duplicated again — and linted by
`tools/audit_page_param.py`: **103 of 1,431 paged rows are that shape**. That
is rows the defect could touch, not proven loss; whether an upstream binds the
first or the last occurrence decides whether it lost anything.

**A declared `id_keys` lost to the flatten cap was invisible**
(`lib/hpengine.c`). When a record is large enough that `hp_flatten` stops at
`HP_MAX_PROPS` before reaching the declared key, the record falls back to the
generic list and then to its first scalar — on a statistical document that is a
*dimension label*, identical across records, so they collapse at the sink while
the run reports an ordinary uid collision. Found on `IE_CSO_COLLECTION`: 13,008
records, 13,008 distinct declared keys, 9 lost. Now counted and disclosed as a
`collector-shape-notice` of kind `id-keys-truncated`.

**A 2xx with an empty body counted as success** (`lib/rss_atom.c`).
`europarl-news` answers `202` with zero bytes on every path and every retry (a
WAF holding pattern) and the run reported `rc=0 records=0` — indistinguishable
from a feed that published nothing. No RSS or Atom document is zero bytes; it
is now a failed fetch, which backoff and quarantine can see.

**The per-host User-Agent table never reached the RSS fleet**
(`core/httpclient.c`, `lib/rss_atom.c`). A `User-Agent:` request header
outranks `CURLOPT_USERAGENT`, and `rss_atom.c` always set one — so the table
built to route around single-token bot filters did not apply to the path with
the most bot-wall trouble in the tree. It now asks `http_ua_override()` and
substitutes only where an entry exists, leaving every already-verified feed on
the agent it was verified with. Two entries added, each with its bisection:
`data.boston.gov` (the rejected token is the word "collector", as at INE — 35
`us-data-boston-gov-*` sources were storing nothing on every run) and
`pib.gov.in` (the rejected token is the contact URL; `"JapanOSINT/1.0 (contact
via repo issues)"` returns 200 with 20 items, so identity *and* a contact route
are kept and only the repo URL is dropped).

---

## 4. Reported, not fixed

- `lib/jsonlist.c`'s `PAGERS` pairs `limit` only with `offset`/`skip`; Socrata's
  metadata API honours `limit`+`page`. The per-row workaround (declaring a
  `page_size` the upstream ignores, to name the cursor family) works but is
  obscure — VJSON rows want an explicit cursor declaration.
- `lib/pagewalk.c`'s `pw_next_link()` rejects *relative* next links and does not
  know `page_meta.next`; `lib/jsonlist.c`'s own `next_link()` resolves relative
  links. Two implementations of one idea that disagree — the pattern CLAUDE.md
  §4b warns about. ChEMBL needed a hand-added `offset=0` because of it.
- `lib/hpengine.c`'s `hp_xml_flatten()` keeps only the FIRST occurrence of a
  repeated child element; PRSN publishes three `<dc:subject>` per item and two
  are dropped. `hp_run_recjar` already joins repeated keys with `"; "`.
- No HTTP-version control in the ABI, and no `CURLE_HTTP2` fallback — two live
  PanamaCompra endpoints are unreachable for that reason alone.
- `CR_SNIT_DISTRITOS` asks a WFS 1.1.0 layer for `maxFeatures=100` of ~490
  districts, and those 100 are already 21.9 MB. Exhaustiveness needs a WFS 2.0
  re-point (`startIndex`), not a bigger cap.
- **The generated files now carry hand edits their manifests do not.**
  `hp3b19_{iana,sigint,eurasia,latam}.c`, `vsrc14_us_state_{1,2}.c`,
  `vsrc15_us_opendata_1.c`, `vsrc2_science_1.c`, `vsrc2_space_1.c` were edited
  directly, as every previous round did. A regeneration would revert all of it,
  silently. Either mirror the opts back into
  `docs/candidate-sources-batch*.txt`, or make the generators refuse to
  overwrite C that differs from what they would emit.
- `ca-gov-news` and a row in `vsrc_government_1.c` poll the same unfiltered
  Canada feed at different page sizes; `lint-sources` cannot see it because the
  query strings differ. Retiring one is a human call.

---

## 5. Verification

Full rebuild from scratch: **0 errors, 0 warnings** under `-Wall -Wextra`.
All seven CI gates green — `selftest` PASS, `unit`, `hptest` (including the new
9f-bis pair), `lint-sources` OK, `audit-sources` **0 findings across the whole
tree**, `pagewalktest`, `source-floor` 16,367 ≥ 16,271.

Note on `make unit`: the target runs `tests/unit/run.sh`, which defaults to
`OBJDIR=obj` — the repo's own object tree. If you built into a different
`OBJ=`, unit links the **stale** objects and fails with errors that have
nothing to do with your change. Export `OBJDIR` to match, or rebuild `obj/`.

The 213 ids were re-swept against the rebuilt binary with
`tools/audit_registry_emit.py`. **These numbers, not any agent's self-report,
are the result of this round:**

| | before | after |
|---|--:|--:|
| OK | 0 | **129** |
| EMITS_NOTHING | 101 | 68 |
| COLLISION | 112 | 14 |
| SLOW | 0 | 2 |
| collision loss per pass | 89,098 | **28,480** |

85 COLLISION → OK, 44 EMITS_NOTHING → OK, one row went from emitting nothing to
emitting-with-collisions (progress, now in the triage pool), and two moved into
`SLOW` — unmeasured at the 220 s kill line, not failed. These 213 ids now store
**573,813 records per pass** (602,257 emitted).

The residual 28,480 is almost entirely *not* loss, and each piece is documented
on its row: `gr-diavgeia-positions` 24,955 (upstream republishes one record
2,989×), the three NASA Exoplanet rows ~3,115 (measured as genuinely duplicate
rows in the full table), IBAMA 185, `UA_OPENPROCUREMENT` 104 (offset cursor
re-serves), MCC/MNC 41, DK MastedB 40. What is left after those is under 50
records, of which `IE_CSO_COLLECTION`'s 9 are the flatten-cap case now
disclosed as `id-keys-truncated`.

Two results worth calling out because a row-level agent could not have produced
them:

- **All 35 `us-data-boston-gov-*` rows are OK** (datasets 235/235, activity
  2000/2001, every organisation row 1:1). The agent that reached them correctly
  reported them as unfixable *from a collector file* — the fix was one entry in
  `core/httpclient.c`'s host table, and the sweep is what proves it worked.
- The 31 `us-socrata-metadata-*` rows went from 100 stored to their real sizes
  (bts 596, chronicdata 1,473, cambridge 447 — each 1:1 emitted to stored).

68 rows still emit nothing, and that is the honest floor for this set: 37 are
CelesTrak (host does not complete a TCP handshake), and most of the rest are
bot walls, dead feeds and two TLS/HTTP-2 failures, every one re-checked against
the site's current feed index before being left alone.
