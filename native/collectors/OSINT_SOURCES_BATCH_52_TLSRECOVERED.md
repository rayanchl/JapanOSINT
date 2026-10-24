# Batch 52 — 19 sources recovered from a TLS chain fault

Three beats, 19 rows, every one authored in 2026-08 as a batch-17 candidate and
rejected by the probe before it could be read, because nine hosts serve their
leaf certificate WITHOUT the public intermediate that signed it. The fault and
its fix are `docs/tls-intermediates-2026-10-21.md`; this file is about the rows.

| file | beat | rows | upstream |
| --- | --- | --- | --- |
| `hp3b52_ansd.c` | ansd | 14 | ANSD Senegal, Drupal JSON:API |
| `hp3b52_hklegco.c` | hklegco | 3 | Legislative Council of Hong Kong, OData |
| `hp3b52_statsgov.c` | statsgov | 2 | ONS Algeria (RSS), PCRWR Pakistan (WP REST) |

Four more rows went back into `hp3b41_jpcorpwp.c` under their original
`JO41_WP_CORP_*` ids rather than into a batch-52 file, because every sibling
row of theirs lives there and that is where a reader would look. Their table
was SPLICED, not regenerated — the shipped file is the maintained copy
(CLAUDE.md, "Batch tooling"), so the four blocks were generated into a scratch
file from the five manifest lines and merged in id order: 103 insertions, 0
deletions.

## Every number below is a measurement, taken 2026-10-21

### ANSD Senegal — walked to the end, 1,402 records

Drupal JSON:API publishes `links.next.href` and stops publishing it on the last
page, so `next_path = "links.next.href"` reads each collection whole. Each
collection was walked to its end before the row was written, and the engine
then stored exactly that number:

    collection                        walked  engine stored
    flash_stat                           581  581
    situation_economique_et_social       570  570
    enquete_et_etude                     101  101
    actualite                             65   65
    lois_et_reglements                    28   28
    rapport_d_activite_ansd               19   19
    appel_d_offre                          8    8
    procedures                             7    7
    evenements / rapport_recensement /
      ressource                            5    5  (each)
    indicateur_economique                  4    4
    concours_et_formations                 3    3
    statistique_theme                      1    1

**No detail hop is declared, and that is a measurement rather than an
omission.** `/jsonapi/node/<type>/{uuid}` returns the same 31 attributes and
the same relationships the list page already carried — asked for one record and
compared field by field — so a second hop would spend a request per record for
nothing. Rule 2 is about not discarding what a source gave back; there is
nothing behind these list hits to discard.

Note what the FIRST PAGE would have suggested: `actualite` hands over 47
records on page 1 with `meta.omitted` explaining that some nodes are withheld
for authorization, and the collection is 65. `flash_stat` hands over 50 — a
round number against a page limit of 50 — and holds 581. A first-page count is
not a collection size.

### LegCo Hong Kong — the service publishes its own total, once asked

OData v3. `$inlinecount=allpages` makes it answer `odata.count`, and **the
engine could not read that key until this batch** — `dotted()` split every path
on `.`, so a key whose NAME contains a dot was unreachable. That also hid
`@odata.nextLink`, which had been in the next-link list all along under a
comment naming OData as supported. Both are fixed; `hptest` 47 and 47b.

    row                        upstream total   this run
    hk-legco-bills                      6,884   6,884 of 6,884 over 36 pages,
                                                complete, no notice
    hk-legco-hansard-speeches         551,619   20,000 a run, cycling, 919.0s
    hk-legco-voting-results           542,641   20,000 a run, cycling, 199.4s

An OFFSET walk with no stable sort silently skips records, so each row asks for
`$orderby` on its own key and page 2 was checked against page 1 for overlap:
**zero of 200** on all three. The two large rows cycle (`page_resume = 1`) with
an ASCENDING sort, so new records land at the tail where a forward walk has not
been yet — the rule 22 rows were re-sorted for on 2026-10-18.

`hk-legco-voting-results` declares **no detail hop** although the service
offers `vVotingResult(<id>)`: asked for one record, it returns the same 49
fields the list row already carried, identical value for value.

### ONS Algeria and PCRWR Pakistan

`ons-algerie-rss` is SPIP's `page=backend`, which publishes the newest ten
items and offers no paging of any kind — a publication tail read daily, not a
collection with an end, and the row says so. 10 of 10 stored.

`pk-pcrwr-posts` replaces the batch-17 candidate for that host. That candidate
was `/wp/v2/search?search=water`: 50 ranked hits carrying id, title, url, type
and subtype, a keyword frozen at authoring time. The same site's `/wp/v2/posts`
holds 134 complete posts with `content.rendered`, which is a superset of those
50, so the collection is registered and the search is not
(`docs/rejected-sources-batch52.tsv`).

## The two candidates that passed every gate and were not registered

Both in `docs/rejected-sources-batch52.tsv`, and neither is visible to a record
count:

* `JO41_WP_CORP_3939` (`www.kanamic.net`) — a live WordPress REST whose entire
  posts collection is one post, dated 2025-10-16, titled **"Hello world!"**.
  The installer's default, which would be stored for ever as a listed
  company's corporate notices.
* `pk-pcrwr-search` — superseded by the collection above.

## Gates

    audit_batch_reachable.py     0 of 19 rows can never run
    audit_batch_pagination.py    0 of 19 declare no pagination but look paged
    batch_exclusions.py --check  0 collisions, 0 near-misses (id set from the binary)
    probe_hp_batch.py --check-filter   19/19 PASS
    audit_registry_emit.py       23 OK, 49,289 emitted = 49,289 stored, 0 uid
                                 collisions. docs/emit-sources-batch52-2026-10-21.tsv,
                                 and -slow- for the one row re-measured alone:
                                 SLOW at the tool's 600 s default with 12,400
                                 already stored, OK at 2,400 s in 919.0 s. A
                                 kill line is a property of the tool.

`core/collectors.def` gained `hp3b52_ansd`, `hp3b52_hklegco` and
`hp3b52_statsgov` through `make collectors-def`; 600 collectors, 0 unassigned.
