# The HTTP 403 class, bisected: the agent was the wrong lever (2026-11-05)

Per-row readings: `docs/http403-agent-sweep-2026-11-05.tsv` (738 rows, 555 hosts).

The complete registry sweep of 2026-10-25
(`docs/registry-health-sweep-2026-10-25.tsv`) left **738 rows whose measured
status was HTTP 403**, and its own handoff named them the highest-value
actionable group for one good reason: on 2026-10-20 the browser-compatible
User-Agent recovered **491 of 523** rows that had answered 403 to the engine's
client. The same lever, applied to the same-looking class, should have worked.

**It did not, and the size of the gap is the finding.** Of the 738 rows, the
agent ladder `core/host_agents.def` prescribes recovers **four**.

## What was measured

Every one of the 555 hosts behind those 738 rows was walked through the whole
ladder, cheapest change first, stopping at the first 2xx — the order the table
itself documents: the engine agent, `HA_NOCOLLECTOR` (the same agent minus the
word "collector"), `HA_CONTACT` (minus the contact URL), `HA_COMPAT` (a
browser-compatible prefix, still naming us), `HA_BROWSER` (a plain Chrome
agent, the repository owner's 2026-10-21 decision, for listed hosts only).

The engine agent is re-asked FIRST, deliberately: the 403s were nine days old
and "one probe is one reading", so a host that recovered on its own must not be
given a table entry it does not need.

| hosts | rows | verdict | |
| --- | --- | --- | --- |
| 304 | 404 | `REFUSES_ALL` | refused all five agents, the browser profile included |
| 214 | 295 | `SELF_HEALED` | answer the ENGINE agent again — the 403 was stale |
| 20 | 21 | `LOCAL_EGRESS` | this container's own egress; **unmeasured from here**, not a finding |
| 17 | 18 | `RECOVERED` | an agent recovered them… on one reading |

## The 17 recoveries became 4, and that is the method working

Each of the 17 was re-measured twice with the engine agent and twice with the
agent that had worked. Only 6 held:

* **8 were the HOST flapping** — the engine agent answers 2xx on a second
  reading, so the first "recovery" was noise. An entry for one of those would
  have put a line in that table for ever, for nothing.
* **3 no longer recover** under any agent at all.

Then the engine cut 6 to 4, which is the half worth reading:

> **`www.rakuten-bank.co.jp` answers 200 to `HA_CONTACT` twice through the
> sweep's own HTTP client and 403 to ALL FOUR agents through the ENGINE**,
> browser profile included.

`core/host_agents.def`'s own note already records the cause — the hosts in that
block judge the engine's `Accept`/`Accept-Encoding` headers, not the agent
alone — and CLAUDE.md's rule decides it: the engine's measurement outranks the
probe, in both directions. That host is a refuses-all, its row keeps
disclosing, and no line claims otherwise. (`www.kansai-airport.or.jp` is the
mirror image: it already had the right entry, so its 403 in the October sweep
was stale and nothing needed doing.)

The four that shipped are engine-measured — each row stored only its own
fetch-failure disclosure before and stores real records now:

| host | agent | row | before → after |
| --- | --- | --- | --- |
| `web.reunionprospectivecompetences.org` | `HA_NOCOLLECTOR` | `JO47_FR_003C9A99…` | disclosure → **10 records** |
| `www.limousis.fr` | `HA_NOCOLLECTOR` | `JO47_FR_2D096A7E…` | disclosure → **1** |
| `www.mairie-esperce.fr` | `HA_NOCOLLECTOR` | `JO47_FR_2176A5CA…` | disclosure → **2** |
| `www.salemcountynj.gov` | `HA_NOCOLLECTOR` | `JO44_US_SALEMCOUNTYNJ_GOV` | disclosure → **10** |

Neither needed more than the two cheapest changes in the ladder. Not one needed
`HA_COMPAT`, which is what the October cohort was recovered with.

## The 304 that refuse everything are a CHALLENGE wall

60 of them were sampled for what they actually serve, because the status never
names the cause:

    54 of 60   Cloudflare (server header)
    52 of 60   a JavaScript challenge interstitial
     3 of 60   a Cloudflare block/captcha page
     6 of 60   unclassified — a plain nginx 403, a CloudFront deny, three
               lacounty.gov WAF pages

A challenge wants JavaScript executed. No User-Agent reaches it, so the
engine's standing remedy for a 403 — *"check whether the row needs a credential
(key_env) or a User-Agent it is not sending"* — was pointing every reader of
those 404 rows at a knob that cannot move, which this tree calls worse than no
remedy at all. `lib/hpengine.c` names the shape now (`hptest` 59–59d).

### The marker that discriminates, and the obvious one that does not

The engine's wall vocabulary ran ONLY where a 200 failed to parse, so a wall
served as 403 never reached it. Extending it needed a marker that separates a
challenge from an ordinary Cloudflare 403 — and the obvious candidate is a
false positive on exactly the hosts the ladder fixes. Measured over 50
refuses-all hosts against the 6 confirmed agent recoveries:

| marker | refused all | agent-fixable | discriminates? |
| --- | --- | --- | --- |
| `cf_chl_opt` | 45/50 | **0/6** | yes |
| `cdn-cgi/challenge-platform` | 46/50 | **1/6** | **no** |
| `challenge-error` | 45/50 | 0/6 | yes |
| `cf-browser-verification` | **0/50** | 0/6 | dead |
| `captcha` | 3/50 | 0/6 | rare |
| `attention required` | 3/50 | 0/6 | rare |

Cloudflare injects the `challenge-platform` script into ORDINARY error pages
too: `www.salemcountynj.gov` serves a plain 1,901-byte "403 Forbidden" carrying
that tag, and the agent minus the word "collector" fetches its feed 200 twice
over. Keying on it would have told the reader not to try the one thing that
works — so `hp_body_is_challenge()` keys on `cf_chl_opt`, and `hptest` 59c is
that exact body, pinned as a control.

**And the engine's existing markers measured 0 of 50.** `cf-browser-verification`
names Cloudflare's previous generation and appears nowhere in this cohort, so
the wall went unnamed even on the 200 path the detector did cover. That is a
second defect, independent of the 4xx one, and `hptest` 59d pins it.

## The 295 self-healed rows: nothing to fix, and that is the point

20 were sampled through the real engine: **17 store real records** (10, 44, 94,
262 …), two transfer 33 KB and find no records in it (the `EMITS_NOTHING`
shape, a different defect) and one is a transport failure. So roughly 250 of
the 738 rows were recovered by elapsed time alone.

That is "one probe is one reading" at the scale of a whole failure class, and
it is the reason the engine agent is re-asked before any entry is written: had
the sweep gone straight to `HA_COMPAT`, it would have "recovered" ~214 hosts
and written ~214 table lines that do nothing, each one a permanent claim about
a host that never needed it.

## What is left, stated rather than implied

* **404 rows behind a challenge wall.** Not an agent problem and not a pacing
  problem. The route to them is a different endpoint the publisher offers — a
  documented API, a bulk file, a mirror — which is per-row work, or they stay
  disclosing. They disclose accurately now, which they did not before.
* **21 rows on 20 hosts that only ever answered with an egress-shaped
  failure.** Unmeasured from here; re-read from a network without this
  container's gateway before treating any of them as a finding.
* **3 hosts whose recovery was not reproducible** and 8 whose hosts flap: no
  entries, and the TSV says so per row.
