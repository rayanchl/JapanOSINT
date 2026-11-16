<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): browser profile, per-host User-Agent table, SEC contact, LegCo and TLS readings. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

**Browser profile for hosts that refuse every honest agent** (2026-10-21, the
repository owner's decision, reversing the earlier "never impersonate a
browser" rule FOR LISTED HOSTS ONLY): `HA_BROWSER` entries in
`core/host_agents.def` (below). `http_request()` gives a request to such a host a browser User-Agent plus
`Accept-Language` and `Sec-Fetch-*` (replacing any `User-Agent` the caller set)
and keeps the caller's `Accept` — NOAA answers an HTML `Accept` with a 202
challenge, and `www.bls.gov` needs the `Sec-Fetch-*` headers, not just the
agent. Every other host still identifies itself. Admit a host only after
bisecting it: engine agent, the agent without "collector", a bare
`JapanOSINT/1.0`, the `Mozilla/5.0 (compatible; JapanOSINT/1.0; …)` prefix —
and give the host the cheapest agent that passes. Listing `registry.faa.gov` revived `FAA_REGISTRY` (31 fields:
owner, address, Mode S) and `US_FAA_AIRCRAFT_REGISTRY`, which was asking for
`Nametxt=` (the form) where `nametxt=…&sort_option=1` returns results: 1,729
aircraft for one operator over 36 pages, keyed `N-Number+Serial Number` because
an N-number can hold a reservation row and an aircraft row. Since 2026-10-21
each hit also fetches its own N-number page (`detail_html_kv`, below), so a
record carries the full registration (37 fields for an aircraft: owner and
address, Mode S octal and hex, engine, airworthiness, certificate dates; a
reservation's own fields for a reserved number). Measured: one operator's
aircraft, 37 of 37 fields; SOUTHWEST AIRLINES, 743 pages fetched in the
120 s detail budget and 986 marked `_detail_pending` with the notice saying so. A third FAA row,
`FAA_NNUMBER_REGISTRY`, read the same page as anchors — once the 403 lifted it
stored the site's navigation links as aircraft, a confident wrong answer — and
is removed as a duplicate of `FAA_REGISTRY`. **When a fix lets a refused row
through, read what it stores**: a 403 notice is honest, a page of nav links is
not. And `sec-reliefweb-headlines` was never an agent problem: its URL 301s to
plain `http://`, where a firewall answers a zero-byte 202; the curl CLI hid it.

**One per-host User-Agent table: `core/host_agents.def`** (2026-10-21). It was
three mechanisms with three precedences — `UA_OVERRIDE` and `BROWSER_HOSTS` in
`core/httpclient.c`, and 471 per-row `.headers` User-Agents on 320 hosts
(batches 44–48), where the row's header beat `UA_OVERRIDE` and `BROWSER_HOSTS`
beat both. Now one line per host names one agent kind (`HA_NOCOLLECTOR`,
`HA_CONTACT`, `HA_RCORP`, `HA_COMPAT`, `HA_BROWSER`), and for a listed host the
table's agent goes on the wire whatever a row set; an unlisted host keeps a
row's own header, else `JO_USER_AGENT`. 403 hosts. `./bin/japanosint
--host-agents` prints it (host, kind, agent, extra headers), and
`tools/probe_hp_batch.py` reads the same file, so the probe sends what the
engine sends. Measured before the move: no other source fetches from the 320
compat hosts, so the wire did not change, and the emit sweep of all 586 rows on
listed hosts read 562 OK; the rest failed under every agent tried, including
the old one (down hosts, 403 walls, two browser-profile hosts refusing this
environment that day). The move dropped `midjourneyvideo.net`: a gambling site
on a city's lapsed domain, whose row had already been removed as a takeover,
was still in `UA_OVERRIDE`. `tests/unit/test_host_agents.c` pins the
resolution.

**SEC is the one host family that refuses us for having NO contact mailbox**
(2026-10-24, `HA_SEC` in that table). Bisected against
`www.sec.gov/files/company_tickers.json` and `data.sec.gov/api/xbrl/frames/…`:
the engine's own agent — product, version, repository URL — is **403 on both**
("undeclared automated tool"), and every agent carrying an `@` address answers
200. So it is not a token the filter dislikes (INE's "collector", HDX's
"OSINT") but the absence of an address someone answers, which SEC's
automated-access policy asks every client to declare. That agent had been in
NINE places — a `#define` in `collectors/sources/sec_edgar.c`, another in
`sec_filing_diff.c`, and a `.headers` User-Agent on each of seven rows — and
the two collectors announced **`contact@example.com`**, which passes the filter
and is not a contact route. One line per host now (`HA_SEC`), and **`$SEC_CONTACT_UA` replaces it**
for a deployment that is not this one. That variable used to be read inside
`sec_filing_diff.c`, where the per-host table would then have overruled it —
an operator knob a later change silently defeated, exactly the shape
`$JO_PAGE_MAX` was in until 2026-10-22 — so it resolves in ONE function,
`host_agent_ua()`, which the dump, the override accessor, the request builder
and `probe_hp_batch.py` all read. Measured after, all nine SEC sources store:
`SEC_EDGAR_FRAMES` 2,679, `SEC_EDGAR_CIK_DIRECTORY_PIVOT` 2,270,
`SEC_EDGAR_SEARCH` 1,260, `SEC_COMPANY_FACTS` 338, the four EDGAR listings
320/53/37/32/31, `SEC_RISK_FACTOR_DIFF` 1 — and unchanged with the variable
set. **A 403 that every honest agent draws is not always a token: ask whether
the host wants a contact address.**

**THE CONTACT IS NOW A CREDENTIAL, `SEC_CONTACT`** (2026-11-16, the owner's
decision; no personal address is in the tree any more). `HA_SEC`'s string is a
template (`JapanOSINT/1.0 research (%CONTACT%)`) filled by `http_contact_ua()`
from `$SEC_CONTACT_UA` (full string, still wins) else `SEC_CONTACT` (an address
or a full agent string), which resolves like any API key: `PUT /api/keys/
SEC_CONTACT` (validated at set time: an `@` address, no CR/LF; a 400 otherwise)
writes `data/api-keys.json` and the process environment. **Unset, no request
goes to an SEC host**: `http_request()` refuses it with a named reason
(`http_credential_refusal()`), and hpengine rows, the three bespoke SEC
collectors and, as the catch-all, `core/scheduler.c` file the same
`collector-status-notice` a key-gated source does (`audit_registry_emit.py`
reads it as `GATED`). `--host-agents` shows `(unset: SEC_CONTACT)`. The same
sweep moved the two Unpaywall rows onto `key_env = UNPAYWALL_EMAIL` and dropped
the optional `mailto=` from the Crossref/OpenAlex rows.

**Two sessions measured SEC on the same day and agree**, which is worth more
than either reading alone. The agent kept is the one that passed all THREE
hosts (`"JapanOSINT/1.0 research (<the owner's address>)"`, main's); the second
reading adds that `OSINTsaas/1.0 (contact@example.com)` and a bare `JapanOSINT
<address>` also pass, so what the filter wants is an `@` address — and main's
`HA_CONTACT` row (200 / 403 / 200 across companyfacts, the Archives
daily-index and the efts search) shows it is enforced per PATH, not per host.
Main's pass also found six more rows announcing `contact@japanosint.local` or
`osint@example.org`, addresses nobody reads, beside the seven that carried the
real one. One entry per host covers all of them.

**And LegCo is not unreachable — its HTTPS is, from this container.** Its three
OData feeds are now `collectors/pivot/table/hp3_legco.c`, read whole with
per-feed page sizes and timeouts measured against a slow server (bills 6,884 in
69 pages a day; votes 542,997 on an `id gt` keyset at 1,000 a page; speeches
551,619 by `$skip` 500 at a time over a ~9-day cycle), and the fourth row — a
second reading of the bills feed at 25 a page — is gone. What this branch
measured is the SCHEME: `https://app.legco.gov.hk` and
`https://www.legco.gov.hk` both end in a TLS `unknown CA` alert under the
session CA bundle AND the system bundle, while `http://www.legco.gov.hk/` 307s
to that same unreachable https; the agent proxy reports
`bundleCoversEveryHost: true` and no relay failure for the host, so that is a
reading of this path rather than of LegCo. **Those URLs are on plain `http`
deliberately: "upgrading" the scheme on inference takes every LegCo row dark.**
Re-take the reading from an environment with ordinary egress before changing
them. `api.data.gov.hk` does answer over https from here, if those endpoints
ever stop.

**Control the TLS reading against a host you know works — and against the
publisher's own chain** (2026-10-25, the correction the PR #51 session relayed,
narrowed by running it). TLS in these containers terminates at the egress
gateway, which presents its own `O = Anthropic` certificate, so a
`no_local_issuer` seen in-container can be about the gateway rather than the
publisher. But it is not about the gateway for EVERY host either: measured the
same minute, `openssl s_client` (which does not use the proxy) verifies
github.com, api.gleif.org AND app.legco.gov.hk — `CN = *.legco.gov.hk`,
`Verify return code: 0 (ok)` — while `curl` through the proxy answers 200 for
gleif, 400 for github's bare root and **000 for LegCo**. So the honest
statement is the narrow one: this path mishandles THAT host, the publisher's
chain is fine, and neither the gateway nor LegCo is at fault on its own. Open
item D6 in `docs/OPEN_ITEMS_2026-10-20.md` bounds the 2026-10-21
AIA/intermediates work the same way. **Two controls, not one: a host known to
work through the proxy, and the publisher read directly.**

