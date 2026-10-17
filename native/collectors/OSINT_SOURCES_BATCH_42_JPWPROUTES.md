# Batch 42 — what each Japanese WordPress host says it has

**1,916 rows, every one measured end to end.** Probe, emit and **stored** all ran
for real, so each row in `docs/verified-sources-batch42.tsv` carries both numbers:
**135,604 emitted, 135,607 stored** over one pass of all 1,916 (stored exceeds
emitted by the per-run disclosure notices). Registry 19,321 → **21,237**.

| Beat | Rows | What |
|---|---:|---|
| `hp3b42_jpcptcorp.c` | 912 | Custom post types on listed-company sites |
| `hp3b42_jppages.c` | 409 | `wp/v2/pages` — the static content each site publishes |
| `hp3b42_jpusers.c` | 347 | `wp/v2/users` — the author/staff register each site publishes |
| `hp3b42_jpcptuni.c` | 205 | Custom post types on university sites |
| `hp3b42_jpcptmuni.c` | 22 | Custom post types on municipal sites |
| `hp3b42_jpcptbank.c` | 21 | Custom post types on financial-institution sites |

**780 distinct record types** — `cpt-news` (80), `cpt-ir-news` (26),
`cpt-products` (19), `cpt-faq`, `cpt-case`, `cpt-topics`, `cpt-officer`,
`cpt-csr`, `cpt-recruit`, `cpt-teacher`, and a long tail of one-offs.

## The method: ask the host, don't guess the path

Batch 41 asked each Japanese WordPress host for exactly ONE collection,
`wp/v2/posts`, because that is the path everybody knows. A WordPress site
publishes its whole route table at `/wp-json`, and what sits in it is the part
that was never asked for: the custom post types a company registers for its IR
releases and product lines, a university for its lecture notices and staff.

Measured 2026-10-12:

| Step | Result |
|---|---|
| Hosts asked for their route table | 461 → **433 answered** |
| Collection routes advertised | **6,726**, 1,570 distinct |
| After dropping editor/plugin machinery and taxonomy term lists | 2,866 probed |
| Returned ≥1 record | **2,567** (216 EMPTY, 59 HTTP error, 13 transport, 11 not-a-list) |
| Staged as manifest rows (media excluded, below) | 2,233 |
| `probe_hp_batch.py` | **1,917 PASS**, 302 HTTP_ERR, 7 NET_ERR, 7 UNPARSEABLE |
| `audit_registry_emit.py --match '^JO42_'` | **1,916 OK**, 1 COLLISION (dropped) |

Pass rate by beat: `jpusers` 100% (347 of 348), `jppages` 97%, `jpcptuni` 89%,
`jpcptcorp` 78%, `jpcptbank` 78%, `jpcptmuni` 56%.

**A route in the table is a claim, not a source.** 299 collections the hosts
advertised returned nothing or an error when asked, and a further 316 failed the
repo's own probe. Every rejection is kept with its status in
`docs/rejected-sources-batch42.*.tsv`.

## Two things measurement contradicted

**`users` is not blocked.** The obvious guess is that security plugins close
`wp/v2/users`, since hardening guides tell people to. It passed **347 of 348** —
the highest rate of any beat. What actually refuses a direct GET is the custom
post types: 253 of the corporate ones are advertised in the route table and then
answer with an HTTP error, because a post type can be registered
`show_in_rest` without its controller permitting an unauthenticated read.

**`wp/v2/media` is excluded, and the reason is a lie in the data.** It answered
on 334 hosts and would have been 334 easy rows. But its `X-WP-Total` cannot be
trusted: `www.kyushu-ns.ac.jp` advertises **2,448,891** attachments and then
returns **55** records for `per_page=100` — a SHORT page, which is the end of its
data. A row like that files a truncation notice forever against a number three
orders of magnitude from reality, and attachment metadata is the least of what
these sites publish. All 334 are in `docs/rejected-sources-batch42.media.tsv`
with that measurement, for whoever comes back with a `mime_type` filter aimed at
documents — which is where the value in media actually is.

## `users` needed its own keys, because its records are a different shape

A user record carries `name`, `id`, `link`, `description` and **no date field at
all**. Declaring `date_keys=date` on it — the obvious copy from the post-shaped
beats — would resolve to nothing on every record of all 347 rows. So that beat
declares `title_keys=name`, `body_keys=description` and deliberately no
`date_keys`, and its interval is 86400 rather than 21600: an author register
changes when staff do, not hourly. Verified against a live record before the
manifests were written, not assumed from the WordPress docs.

Everything else — pages and every custom post type — is post-shaped
(`title.rendered`, `date`, `link`, `content.rendered`) and carries batch 41's
key set, including `array_path=.` for the bare root array and `page_max=60`.

## The same host failed the same way twice

`JO42_WP_CORP_6407_PAGES` came back COLLISION: emitted 19, stored 9. That is
`www.ckd.co.jp` (証券コード 6407) — **the host batch 41 dropped for exactly this**,
after measuring that it serves overlapping page contents deterministically so
that records which exist are never served. It does it on `wp/v2/pages` as well
as on `wp/v2/posts`, which settles what batch 41 could only infer: the defect is
a property of that site's WordPress, not of the collection being read. Dropped
with the measurement rather than papered over by widening `id_keys` off the post
id. Nothing else in 1,917 rows collided.

## Gates

```
make                       0 new warnings
make audit-sources         0 findings, strict set 318 files
make lint-sources          OK, 21237 registered
make hptest pagewalktest selftest unit authtest   all pass
tools/ci_concurrency_gate.sh                      ok
source-floor/registry-floor  re-recorded at 21237, 0 duplicates, 0 dropped
batch_exclusions.py --check  0 collisions against all 19,321 prior sources
audit_batch_pagination.py    0 of 2,233 undeclared
audit_batch_reachable.py     0 of 2,233 can never run
```

`make htmlparsetest` is killed (SIGKILL) in the container this was authored in,
and it is **not** this batch's: the diff touches neither `lib/htmlparse.c` nor its
test, that binary links only `htmlparse.c` and `seenset.c`, and a build of the
test from `origin/main` ALONE is killed identically. It needs a memory bound this
container will not give it; the GitHub runners do, and CI passes it.

`--check-filter` does not apply — these are scheduled feeds with no entity token,
so there is no filter to ignore (rule 4d).

## Re-measuring

```sh
cd native
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO42_' \
        --jobs 8 --timeout 200 --out sweep42.tsv
```

The route tables move: a site that adds a post type gains a collection this batch
does not have, and one that removes a plugin loses a row to a 404 that the
fetch-failure notice will then carry. Re-running the `/wp-json` discovery is how
the next pass finds the first kind.
