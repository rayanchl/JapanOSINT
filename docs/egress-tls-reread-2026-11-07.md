# The egress- and TLS-shaped failures, re-read (2026-11-07)

Per-row readings: `docs/egress-tls-reread-2026-11-07.tsv` (1,737 rows).

The 2026-10-25 re-read (`docs/fetch-failed-reread-2026-10-25.tsv`) left 1,737
`FETCH_FAILED` rows that had no HTTP status. Their notes read "TLS handshake",
"NO CERTIFICATE WAS JUDGED", `no_local_issuer` or "transport …", and 21 more
rows came from the 403 sweep's `LOCAL_EGRESS` class. The open item said that
these need a reading from ordinary egress, because TLS for every host ends at
this container's gateway.

That reading is still not available from here. Two things can be measured
from here, though:

* **the engine, again**, through the real binary (`audit_registry_emit.py`,
  12 jobs, 220 s);
* **plain `curl` through the same proxy, twice per host.** A host that curl
  reaches while the engine does not points to a question about the engine. A
  host that neither reaches, with the proxy logging a relay failure, is this
  container's own egress.

## What was measured

| rows | class | |
| --- | --- | --- |
| 850 | `RECOVERED` | store records through the engine now — the failure was 13 days stale |
| 11 | `RECOVERED_ON_SECOND_READ` | failed once more and then stored, on hosts curl reached with 200 |
| 499 | `EGRESS_UNMEASURED` | curl through the proxy fails too (132 hosts `Connection reset by peer`, 9 `CONNECT tunnel failed`; 11 hosts named in the proxy's own relay-failure log). **Unmeasured from here, not findings** |
| 247 | `UPSTREAM_STATUS_*` | the host answers curl with an HTTP status (403 ×138, 404 ×54, 503, 401, 429, 410, 400, 405, …). One request to ONE row's URL per host, so this is a reading of the host, not a verdict on every row |
| 41 | `HOST_ANSWERS_ROW_FAILS` | curl gets 200 and the engine still fails — see below |
| 73 | `UNMAPPED` | bespoke collectors whose request URL is a `#define` the id→URL mapper cannot read; not probed |
| 6 / 6 / 2 | `COLLISION` / `EMITS_NOTHING` / `SLOW` | now fetching; their other verdicts are their own findings |

**Half the class was elapsed time.** 861 of 1,737 rows store records today
with no change to any of them. This is "one probe is one reading" again, at
the same scale as the 403 class (295 of 738 self-healed). Any single sweep's
transport failures are mostly about the moment the sweep ran.

## The 41 that fail while their host answers 200

None of them is an engine defect. In 32 of the 41, every host the collector
contacted answered, and the collector still returned non-zero. Reading the
bodies explains it: `www.brookings.edu/feed/`,
`carnegieendowment.org/posts/rss/`, `www.bcb.gov.br/rss/ultimasnoticias`,
`www.cert-in.org.in/RSS/CertIn.xml` and
`www.caribbeannationalweekly.com/feed/` each answer HTTP 200 with
`text/html` and the site's homepage, and zero `<item>`. CLAUDE.md calls this
a feed URL that has stopped being a feed. `lib/rss_atom.c` returns non-zero
on it, and the scheduler's disclosure names the hosts and the byte count, so
these rows already disclose it correctly. The fix is to
re-point each one at the feed its homepage now declares
(`./bin/japanosint --discover-feeds`), not to change the engine. The other 9
are egress-shaped and flapping: curl reached them once, and the engine's
re-read did not.

## What this does and does not license

* The 499 `EGRESS_UNMEASURED` rows are still unmeasured. Nothing was
  removed or changed on this reading, which is the policy CLAUDE.md applies to
  `LOCAL_EGRESS`.
* The 22 rows that still read `no_local_issuer` are about the gateway's chain
  (open item D6). A cause read here is never the publisher's.
* A handshake reset now carries that caveat in its own remedy
  (`http_tls_cause_remedy(HTTP_TLS_HANDSHAKE)`). It names the egress tunnel as
  one possible cause before the cipher-level advice, so the next reader does
  not need this document to know it.
