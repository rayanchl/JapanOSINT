# The Austrian Parliament filter API, measured (2026-10-23)

Open item B2 has carried `AT_PARLAMENT_RECORDS` since 2026-10-21 as **"one
measured sort parameter away"** from shipping the parliament's own materials
index. This is that measurement. The answer is that **there is no sort
parameter**, and the row ships anyway — because the same session found a route
that needs no ordering.

Everything below is one endpoint:
`https://www.parlament.gv.at/Filter/api/filter/data/101`.

## 1. The query string is ignored. The filter is a POST body.

This single fact explains, and retires, the earlier reading.

| request | `count` |
| --- | --- |
| no parameters | 391,354 |
| `?GP=XXVII` (GET) | 391,354 |
| `?GP_CODE=XXVII`, `?gp=XXVII`, `?FILTER_GP=XXVII`, `?ITYP=VER` (GET) | 391,354 |
| POST body `{"GP":["XXVII"]}` | **76,457** |
| POST body `{"ITYP":["VER"]}` | **316** |

So every sort spelling probed in a query string was being discarded before the
server looked at it.

## 2. There is no sort parameter — and the control is what proves it

26 spellings were sent in the query string (`sort`, `sortColumn`, `sortRnr`,
`sort_rnr`, `sortKey`, `sortBy`, `orderBy`, `order`, `sortField`,
`vtsSortField`, `vts_sort_field`, `sortClause`, `psort`, `pSort`, `sortierung`,
`sortSpalte`, `sortAsc`, `sortDirection`, `sortOrder`, each with a column
number, a column name, or a direction). **14 of them "moved" the first page** —
which reads like a working parameter, and is the shape the 2026-10-21 triage
reported.

Then the control:

| request | first page moved? |
| --- | --- |
| `&sortColumn=99999` | yes |
| `&frobnicate=7` | yes |
| `&zzzzqqqq=1` | no |
| `&notaparam=xyzzy` | no |

A parameter no sort implementation could honour reorders the answer exactly as
`sort=28` does, and `sort=28` read twice gives `inr = 6,1,2,4,1,3,…` and then
`1,9,3,65,144,66,…` — neither of them ordered by the column the parameter
names. The same nine spellings inside the POST body behave the same way, and an
unknown body key is silently ignored there too
(`{"ITYP":["VER"],"zzzunknown":["x"]}` still answers 316).

So: not six working sort parameters, not one. **Probe a control before
believing a parameter works** — this endpoint's answer flips between two
orderings for reasons that have nothing to do with what you asked for.

## 3. `pageNumber` is not the page parameter. `page` is — and it drifts.

`pageNumber=2`, `=3` and `=500` all return the same twenty rows. `page` really
pages: `page=19568` returns the last 14 rows, `page=19569` returns none, and
adjacent pages are disjoint on one read.

But two reads of `page=1` return **completely disjoint sets**, and page 1
against page 2 of ONE filtered query (`{"ITYP":["VER"]}`, 316 rows over 16
pages) **overlapped by 8 of 20 rows**. That is the shape CLAUDE.md calls the
worst it records: a record served twice and another never served, with a
one-record collision as the only visible symptom. No `page_resume` cycle can
sit on this.

## 4. `showAll=true` plus a filter is a complete, deterministic, one-page read

| slice | `pages` | `count` | rows returned | distinct | bytes | time |
| --- | --- | --- | --- | --- | --- | --- |
| `{"GP":["PN"]}` | 1 | 316 | 316 | 316 | 178,315 | — |
| `{"GP":["V"…"XVI","KN","PN","BR"]}` | 1 | 50,896 | 50,896 | 50,896 | 22,311,421 | 4.1 s |
| `{"GP":["XVII"…"XXI"]}` | 1 | 77,646 | 77,646 | 77,646 | 39,497,900 | 5.7 s |
| `{"GP":["XXII","XXIII","XXVI"]}` | 1 | 55,695 | 55,695 | 55,695 | 33,849,967 | 5.2 s |
| `{"GP":["XXIV"]}` | 1 | 51,970 | 51,970 | 51,970 | 31,321,562 | 4.8 s |
| `{"GP":["XXV"]}` | 1 | 51,979 | 51,979 | 51,979 | 30,004,930 | 5.2 s |
| `{"GP":["XXVII"]}` | 1 | 76,457 | 76,457 | 76,457 | 47,692,962 | 5.8 s |
| `{"GP":["XXVIII"]}` | 1 | 26,714 | 26,714 | 26,714 | 16,939,356 | 3.1 s |

The XXVII slice was read twice and came back byte-identical (47,692,962 bytes
both times, same 76,457 keys). `distinct` is on `GP_CODE+ITYP+INR+NRBR` — see
§6.

`export=true` and `export=csv` are accepted and change nothing: the same JSON,
within 2 KB of the plain read.

## 5. The partition is exhaustive, by the upstream's own arithmetic

`GP` is the legislative period. Enumerating Roman numerals I..XXVIII found 24
non-empty codes:

    V 1,903   VI 1,988   VII 1,857   VIII 1,509   IX 1,589   X 2,049
    XI 5,534  XII 2,775  XIII 7,764  XIV 7,134    XV 7,811   XVI 6,768
    XVII 15,158  XVIII 18,180  XIX 6,022  XX 23,343  XXI 14,943
    XXII 18,885  XXIII 15,444  XXIV 51,970  XXV 51,979  XXVI 21,366
    XXVII 76,457  XXVIII 26,711

which sum to **389,139 against an unfiltered 391,354 — 2,215 rows that no Roman
numeral reaches.** Shipping on the Roman numerals alone would have dropped them
in silence, which is exactly the discard rule 2 names.

Sampling 30 pages (600 rows) for distinct `GP_CODE` values found the two codes
that hold them, `KN` (1,897) and `PN` (316) — and **missed a third**, `BR` (2),
which only a direct count found. The 26 codes passed as one filter answer
391,352; plus BR's 2 that is **391,354, the unfiltered count exactly**. So
these 27 codes partition the index with nothing outside them.

*Enumerate the dimension you are slicing on, then check the slices sum to the
whole.* A sample finds the common missing codes and not the rare ones.

## 6. The record key, and why the obvious one is wrong

Over XXVII's 76,457 rows:

| key | distinct |
| --- | --- |
| `WENTRY_ID` (the site's own web-item id) | 75,485 |
| `HIS_URL` | 75,485 |
| `GP_CODE+ITYP+INR` | 75,485 |
| `GP_CODE+ITYP+INR+ZUKZ` | 75,485 |
| `INRNUM` | 19,510 |
| **`GP_CODE+ITYP+INR+NRBR`** | **76,457** |

Three independent-looking keys agreeing on 75,485 is a strong invitation to
write the 972 remaining rows off as "the upstream's own duplicates" — and that
would have been wrong. **All 972 groups DIFFER**, and every one of them differs
in exactly one column: `NRBR`. A material that went through both chambers is
served once as `NR` and once as `BR`, under one web-item id. Adding `NRBR`
gives 76,457 of 76,457, and it holds on every slice in §4.

`INRNUM` is the number as an integer and is a dimension, not an identity — the
shape rule 4b's comma-vs-plus note is about.

## 7. What `array_path` and `row_header_path` have to be

`rows` is an array of ARRAYS; `header` is an array of 36 OBJECTS. The engine's
fallback picks the densest array **of objects**, so without an explicit
`array_path = "rows"` it mines the header and emits 36 column definitions as
parliamentary materials. `row_header_path = "header"` is what names the cells
(`GP_CODE`, `Betreff`, `NRBR`) instead of `.0`, `.6`, `.29`; the Austrian header
entries are objects carrying a `label`, which `hp_header_name()` already reads
(hpengine, 2026-10-22). Four of the 36 columns carry no label and keep their
positional name, which is the engine's documented behaviour.

## 8. The window that was measured and NOT shipped

`DATUM` is a recognised body filter with **exact-value set semantics, not a
range**:

| body | `count` |
| --- | --- |
| `{"DATUM":["03.12.2020"]}` | 136 |
| `{"DATUMSORT":["20201203"]}` | 136 |
| `{"DATUM":["2026-01-01","2026-10-23"]}` | 0 |
| `{"DATUM":{"from":…,"to":…}}` | ignored (391,355) |
| `{"INRNUM":["1","2"]}` | 921 |

So a window is an explicit list of days — which the engine's own
`{date:%d.%m.%Y:-N}` tokens render. Seven such days measured **163 records in
98,572 bytes, `pages: 1`**, complete and distinct.

That row was deliberately not shipped: the `XXVIII` slice already reads the open
period WHOLE on every run at 16.9 MB, so a seven-day row would store the same
records under a second `source_id` — the double-store `lint-sources`'
dup-endpoint check exists to prevent — and buy no freshness. It is written down
because it IS the split to make when the open period outgrows one page: the
previous period reached 76,457 rows and 47.7 MB over its term, and past roughly
90,000 records (the `showAll` single-page cap is 100,000) a slice comes back
`pages: 2`.

## 9. What ships

`native/collectors/pivot/table/hp3b55_atparl.c`, seven rows, grouped so each
stays under the 100,000-row page cap and under `core/httpclient.c`'s 64 MB body
ceiling, and so that no group mixes a closed period with the open one.

One full cycle transfers ~222 MB. The declared interval is weekly for the six
closed slices and daily for the open one, but `core/cadence.c`'s compiled
default (global 6 h) sits above a declared value in both directions, so on an
unconfigured database that is ~890 MB a day. A per-source interval outranks the
policy and is the remedy; the number is here so the decision can be made from a
measurement.

The entity pivot `AT_PARLAMENT_RECORDS` in `hp3b31_eunat.c` is untouched: it
answers "what do we have on X" against the parliament's search page, which is a
different question from reading the index.
