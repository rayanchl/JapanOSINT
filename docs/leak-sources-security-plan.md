# Leak Sources — Master Acquisition & HIBP-Rebuild Plan (for security studies)

> **Purpose.** One place that answers "how do we download leaks and re-do Have I Been Pwned
> for security research?" It catalogs **every leak source class**, says **how each is lawfully
> acquired**, maps each to the **pivot** it feeds and the **collector** that already exists (or is
> net-new), and lays out the phased build.
>
> This is a **plan**, not an acquisition guide. It contains **no** download URLs, magnets,
> torrents, forum names, or instructions for obtaining illicitly-leaked credential dumps. It
> catalogs dataset *identities* and *lawful bulk channels* only — cataloging identity ≠ endorsing
> acquisition. Read the guardrails in §7 before writing any ingest code.
>
> **Companions (do not duplicate them — this doc ties them together):**
> [`breach-check-pipeline.md`](./breach-check-pipeline.md) (the engine design),
> [`breach-sources.md`](./breach-sources.md) (dataset-identity catalog + lawful channels),
> [`breach-ingest-revamp-plan.md`](./breach-ingest-revamp-plan.md) (bulk-materialize throughput),
> [`breach-corpus.seed.tsv`](./breach-corpus.seed.tsv) (1,171 breach identities),
> [`osint-sources-1000.md`](./osint-sources-1000.md) §7/§8/§10–12/§20 (the provider ecosystem).

---

## 0. TL;DR — the shape of the answer

Two planes, kept strictly separate:

1. **Offline corpus plane (the actual HIBP rebuild).** We ingest breach *datasets we lawfully hold*
   once, ahead of time, into a hashed, deduped, prefix-sharded local index. Lookups touch **only**
   this index — **zero query-time network calls**. This is the engine that already exists in
   `native/core/breach_index.c` + the four `BREACH_INDEX_*` collectors. The **one** genuinely
   bulk-downloadable dataset is **Pwned Passwords** (free, licensed for exactly this); everything
   else on this plane is **licensed feeds** or **data you are authorized to hold**.

2. **Live-pivot plane (enrichment, never the request path).** Third-party breach/paste/darkweb
   APIs (HIBP, DeHashed, LeakCheck, Hudson Rock, XposedOrNot, Ransomlook, psbdmp, …) — many already
   wired as collectors. Used for **metadata, pivots, and monitoring**, never to source the
   credential corpus and never as a per-lookup dependency for the offline index.

The plan below enumerates **all** leak sources across both planes, tiers them by legal risk, and
says what to build.

---

## 1. What already exists (reuse — do not rebuild)

The tree already has a working, offline, privacy-preserving breach subsystem. Grounded inventory:

| Piece | File(s) | State |
|---|---|---|
| Offline hashed index (ingest + shard + AES-GCM secret-at-rest + reveal path) | `native/core/breach_index.c` (466 LOC) | **Built.** Text shards `data/breach/<type>/<xx>.idx`; `--ingest` in `main.c`. |
| Four offline lookup collectors | `breach_index_svc.c` → `BREACH_INDEX_{EMAIL,USERNAME,PHONE,PASSWORD}` | **Built.** `reveal=0`, emits `intel_item`. |
| Breach-identity corpus manifest | `breach-corpus.seed.tsv` (1,171), `breach-corpus.json` (stale @1,018 — regenerate) | **Data present.** Seeds `breach_meta`. |
| Corpus loader collector | `breach_corpus.c` → `BREACH_CORPUS` | **Built.** |
| Password k-anon client (HIBP range, SHA-1 5-prefix) | `password_checker.c` → `api.pwnedpasswords.com/range/` | **Built.** Mirror for our own range API. |
| Email breach lookup (live, off offline path) | `breach_checker.c` → HIBP `breachedaccount` | **Built, NEEDS_KEY** (`HIBP_API_KEY`). |
| Credential/breach search (live) | `dehashed_search.c` (LeakCheck real; DeHashed half is a stub note), `credential_leak.c` (GitHub code-search) | **Built, partial.** |
| Breach-metadata feed (live) | `cyi_xposedornot_breaches.c` → XposedOrNot `v1/breaches` | **Built.** Keyless breach catalog — good manifest enrichment. |
| Stealer-log intel (live) | `hudson_rock_jp.c` → Cavalier `search-by-domain` | **Built, NEEDS_KEY.** |
| Exposed-service leaks (live) | `leakix_jp.c` → leakix.net | **Built, NEEDS_KEY.** |
| Paste monitoring (live) | `pastebin_monitor.c` → psbdmp.ws + grep.app + web.archive | **Built.** |
| Dark-web / paste monitor (live) | `dark_web_monitor.c` → Ahmia + IntelX + Pastebin | **Built, partial.** |
| Ransomware leak-site tracker (live) | `global_ransomlook.c` → ransomlook.io | **Built.** |
| Investigative leak archives (live) | `global_aleph.c`, `global_icij.c` → OCCRP Aleph / ICIJ | **Built.** |
| Bulk-materialize throughput design (dedicated `breach_items`+`breach_fts`, 100k rows/s) | `breach-ingest-revamp-plan.md` | **Designed, NOT built.** |

**Gaps to close (net-new):** the bulk-materialize path (`--materialize`, dedicated table); our own
k-anonymity **range API** `GET /api/breach/range/{type}/{prefix5}`; the auth-gated owner-reveal
`GET /api/breach/mine`; the sorted-binary `.bin` store for billion-row scale; a licensed-feed
ingest adapter; and wiring XposedOrNot/HIBP metadata into `breach_meta` refresh.

---

## 2. The pivots and what feeds each

The feature answers four identifier pivots. Every source in §3 is classified by which it feeds.

| Pivot | Normalization | Stored | Fed by |
|---|---|---|---|
| **Password** | trim only | `SHA1(pw)` + prevalence count | Pwned Passwords bulk; any breach exposing passwords/hashes; research wordlists |
| **Email** | lowercase+trim | `SHA1(email)` + `enc(secret)` | most breaches; licensed feeds; stealer logs |
| **Username** | lowercase+trim | `SHA1(user)` + `enc(secret)` | forum/gaming/social breaches; stealer logs |
| **Phone** | E.164 | `SHA1(e164)` + `enc(secret)` | telco/delivery/marketing dumps; enrichment sets |

Anonymous lookups return **hit / count / which-breach** only. Plaintext secrets are AES-256-GCM at
rest and returned **only** to the verified owner (`reveal=1`). No master key → reveal impossible
(fail-closed). This is the SpyCloud/Constella model, and it's already how `breach_index.c` behaves.

---

## 3. All leak sources, by class — acquisition, pivot, legal tier, action

**Legal tiers:** **A** = freely + lawfully bulk-downloadable for this use · **B** = licensed
feed / contract · **C** = data you are independently authorized to hold (your own, CERT/DPA/LE) ·
**D** = live query-time API (enrichment/pivot only, **off** the offline corpus) · **X** = **out of
scope** — illicitly-obtained dumps; catalog identity only, never acquire.

### 3.1 Password corpus — the anchor (Tier A)

| Source | Feeds | Mode | Collector | Action |
|---|---|---|---|---|
| **Pwned Passwords** bulk (`SHA1:count`, + NTLM) | Password | Tier A bulk file, free/licensed, already hashed | `password_checker.c` (range client) | **Ingest first.** S2→S6, skip S3/S4 → `data/breach/password/`. Stands up the entire password pivot with zero API. Refresh on version bump. |
| Published research password lists (no tied PII) | Password | Tier A, redistributable | — | Optional prevalence enrichment; **only** password-only lists with no identifier attached. |

This single Tier-A ingest delivers a complete, offline, HIBP-compatible password check. Do it first.

### 3.2 Identity corpus — email / username / phone (Tiers B & C)

| Source | Feeds | Mode | Collector | Action |
|---|---|---|---|---|
| **SpyCloud** recaptured breach + malware/stealer feed | Email/User/Phone | **Tier B** enterprise contract, periodic bulk export | net-new feed adapter | Ingest export like any dump; terms govern retention/redistribution. |
| **Enzoic** compromised-credential datasets + local matching | Email/Password | **Tier B** API/key + offline sets | net-new | Offline sets ingest to identity index; local-match API stays off the request path. |
| **Constella** identity-intel | Email/User/Phone | **Tier B** commercial | net-new | Same adapter shape as SpyCloud. |
| **Your own** breach/customer data | all | **Tier C** (you are controller) | `--ingest` | Direct ingest, provenance-logged. |
| **CERT / DPA / law-enforcement** notification datasets | all | **Tier C** authorized | `--ingest` | Human-approved per §7. |

These are **licensing relationships, not downloads of dumps**. One net-new **feed-ingest adapter**
(auth, paginated/bulk export → the existing S2→S6 pipeline) covers all Tier-B providers.

### 3.3 Breach metadata / manifest enrichment (Tier A/D — labels, not credentials)

Needed so a hit reports *which* breach with a real title/date/count. Metadata only — never the
credential rows.

| Source | Mode | Collector | Action |
|---|---|---|---|
| `breach-corpus.seed.tsv` → `breach-corpus.json` | Tier A committed manifest (offline) | `breach_corpus.c` | Regenerate JSON (stale @1,018 vs 1,171 seed) → seed `breach_meta`. |
| **XposedOrNot** `v1/breaches` | Tier D keyless catalog | `cyi_xposedornot_breaches.c` | Wire into an out-of-band `breach_meta` refresh (names/dates/dataclasses). |
| **HIBP** breach metadata (`/breaches`) | Tier D, keyless for metadata | `breach_checker.c` | Offline one-off manifest refresh only; never per-lookup. |

### 3.4 Stealer / infostealer logs (Tier B/D)

| Source | Feeds | Mode | Collector | Action |
|---|---|---|---|---|
| **Hudson Rock (Cavalier)** | Email/User + URL context | Tier D API/key (`search-by-domain`) | `hudson_rock_jp.c` | Pivot/monitoring; if a bulk feed is contracted → Tier B ingest. |
| SpyCloud malware/stealer records | Email/User/Password | Tier B (part of 3.2) | feed adapter | Ingested with 3.2. |

Stealer/combo aggregates overlap heavily — the **Bloom dedup** (pipeline §S5) is what stops the
index storing the same hash a billion times.

### 3.5 Paste-site & text-leak monitoring (Tier A/D — signal, not corpus)

| Source | Mode | Collector | Action |
|---|---|---|---|
| **psbdmp.ws** historical paste dumps | Tier D API | `pastebin_monitor.c` | Keep. Monitoring/pivot; hits can flag candidates for §7-gated review. |
| grep.app / GitHub code+gist search | Tier D API | `credential_leak.c`, `github_leaks_jp.c`, `pastebin_monitor.c` | Keep. Secret-in-code leak detection. |
| Ahmia / IntelX paste+darkweb | Tier D | `dark_web_monitor.c` | Keep; `dark_web_monitor` is PARTIAL — tighten per §7 (no fabricated labels). |
| Pastebin Scraping API, paste.ee, dpaste, PrivateBin, Rentry (§8) | Tier D web/API | net-new (optional) | Add only if monitoring coverage is wanted; not on the corpus path. |

### 3.6 Dark-web & ransomware leak-site trackers (Tier A/D — monitoring)

| Source | Mode | Collector | Action |
|---|---|---|---|
| **Ransomlook** | Tier D web/API | `global_ransomlook.c` | Keep. Victim/leak-site feed. |
| ransomware.live | Tier D API | net-new (optional) | Add for redundant victim coverage. |
| **OCCRP Aleph**, **ICIJ** | Tier D API/key | `global_aleph.c`, `global_icij.c` | Keep. Published investigative leaks (documents, not creds). |
| DDoSecrets (published leaks) | Tier A/ref published | net-new (optional) | Document-class; ingest only what's lawfully published/held. |
| DarkOwl / Flare / Recorded Future / Cybersixgill / KELA / Webz.io (§20 commercial) | Tier B commercial | net-new (optional) | Only if a contract exists; treat like 3.2 feeds. |

### 3.7 Live breach-search APIs — pivot plane only (Tier D)

Kept in the tree for the LLM-pivot / monitoring flow; **never** wired into the offline lookup
endpoints (§5), **never** a per-lookup dependency of the corpus.

| Source | Collector | Note |
|---|---|---|
| HIBP (email/paste) | `breach_checker.c` | NEEDS `HIBP_API_KEY`. |
| DeHashed / LeakCheck | `dehashed_search.c` | LeakCheck real; DeHashed half is a permanent stub note (honest empty). |
| LeakIX | `leakix_jp.c` | Exposed-service leaks; NEEDS_KEY. |
| Snusbase, LeakCheck, IntelX, BreachDirectory, LeakPeek (§7) | net-new/optional | Add as pivots only if keys available. |

### 3.8 Explicitly out of scope (Tier X)

Cracking forums, "combo"/leak marketplaces, Telegram leak channels, torrent indexes of stolen
dumps, WeLeakInfo-style anonymous plaintext lookup, resale sites. We **name their dataset
identities** in the corpus manifest (so a hit can be *labeled*, e.g. "Collection #1", "Naz.API",
"ALIEN TXTBASE") but provide **no** means of obtaining them and **do not** host data we aren't
authorized to hold. Hosting stolen-credential compilations carries serious, jurisdiction-dependent
legal liability. Get counsel before hosting identity data (§7).

---

## 4. Acquisition → ingest pipeline (existing engine + the gaps)

```
 acquire ─▶ stage/verify ─▶ parse ─▶ normalize ─▶ hash ─▶ dedup ─▶ shard/index ─▶ publish
   (S0)         (S1)         (S2)       (S3)       (S4)     (S5)       (S6)          (S7)
   Tier A/B/C   human gate   bigfile   §2 table   SHA-1    Bloom      data/breach   breach_meta
```

- **S0 Acquire** — one-shot, ahead of time. Tier A = download the bulk file once; Tier B = pull the
  contracted export; Tier C = load authorized data. No per-lookup fetch, ever.
- **S1 Stage & verify** — land raw file in encrypted, access-logged staging **outside** the served
  DB; record sha256/size/row-estimate; **quarantine until a human marks the source `approved`**
  (the legal gate — non-negotiable, §7).
- **S2–S6** — streaming `bigfile_reader`, normalize per §2, SHA-1 (+SHA-256), Bloom dedup + on-disk
  sort/unique, shard by first 2 hex. Pwned Passwords skips S3/S4 (pre-hashed).
- **S7 Publish** — flip job `live`, update `breach_meta`, bump corpus version, write hash-chained
  `audit_events`.

**Scale path (from `breach-ingest-revamp-plan.md`, not yet built):** for full materialization,
add the dedicated `breach_items` + external-content `breach_fts` table, batched transactions
(N≈50k), deferred FTS `rebuild`, bulk PRAGMAs → **≥100k rows/s**. For billion-row lookup, add the
sorted-binary mmap `.bin` store (S6 upgrade). Both are **net-new**; the text-shard path ships today.

---

## 5. Lookup API surface (HIBP privacy model)

All read the **local index only** — no third-party calls on the request path.

- `GET /api/breach/range/{type}/{prefix5}` — **k-anonymity**. Returns `suffix:count` (+ breach-id
  bitmask) for the 5-hex prefix; client matches locally; server never learns the value. **Net-new.**
- `POST /api/breach/check {type,value}` — server-side convenience; value hashed in-process, never
  logged; returns `{found,breaches,count}`. Auth + quota gated.
- `GET /api/breach/breaches` · `GET /api/breach/breach/{id}` — public corpus metadata (like HIBP).
- `GET /api/breach/mine` — **auth-gated, ownership-verified** owner-reveal (`reveal=1`) → decrypted
  own secrets. The "see my own leaked password" feature. **Net-new.**

Existing live `source_def`s (`BREACH_CHECKER`, `DEHASHED_SEARCH`, …) stay for the pivot flow; they
are **not** wired into these endpoints.

---

## 6. Per-source implementation checklist (roll-up of §3)

| # | Source class | Pivot | Tier | Collector status | Next action |
|---|---|---|---|---|---|
| 1 | Pwned Passwords bulk | Password | A | client exists | **Ingest first**; add bulk-file ingest mode |
| 2 | Licensed feeds (SpyCloud/Enzoic/Constella) | E/U/P | B | none | Build **one feed-ingest adapter** |
| 3 | Your own / CERT / DPA / LE data | all | C | `--ingest` | Approve + ingest per §7 |
| 4 | Corpus manifest | metadata | A | `breach_corpus.c` | Regenerate stale JSON → `breach_meta` |
| 5 | XposedOrNot / HIBP metadata | metadata | D | exist | Wire into out-of-band `breach_meta` refresh |
| 6 | Hudson Rock stealer logs | E/U | B/D | `hudson_rock_jp.c` | Keep pivot; ingest if feed contracted |
| 7 | Paste monitors (psbdmp/grep.app/gist) | signal | A/D | exist | Keep; harden `dark_web_monitor` (PARTIAL) |
| 8 | Ransomware/darkweb trackers | monitoring | A/D | ransomlook/aleph/icij exist | Keep; optional ransomware.live |
| 9 | Live breach APIs (HIBP/DeHashed/LeakCheck/LeakIX/…) | pivot | D | mostly exist | Keep off request path; add keys |
| 10 | Illicit dumps | — | **X** | — | **Never acquire**; identity-label only |

**Engine gaps (net-new, independent of sources):** bulk `--materialize` + `breach_items`/`breach_fts`;
k-anon `range` API; owner-reveal `/mine`; sorted-binary `.bin` store; feed-ingest adapter.

---

## 7. Guardrails (non-negotiable — they shape the code)

1. **Lawful acquisition only.** Because everything is pre-downloaded and self-hosted, *where the
   data comes from* is the entire legal exposure. Ingest only Tier A/B/C. Tier X is never acquired.
2. **Human approval gate before ingest.** Datasets stay quarantined until a source is marked
   `approved`; provenance + file sha256 recorded for every job. No auto-ingest of unvetted dumps.
3. **Plaintext only to the verified owner.** Identifiers indexed as hashes; secrets AES-256-GCM at
   rest, returned only via `reveal=1` after ownership verification. No master key → fail-closed.
   There is **no** endpoint returning plaintext for an arbitrary target (the WeLeakInfo model is
   illegal and out of scope).
4. **Sensitive breaches** (`sensitive=1`, e.g. affair/adult/health) never listed by public email
   lookup — only the verified owner sees them (HIBP's rule).
5. **Data-subject rights** — delete-by-hash + suppression list; one-command purge.
6. **Access control + audit** — every check writes a hash-chained `audit_events` row; multi-tenant
   quotas already exist.

### House-rule compliance (CLAUDE.md)

- **Never fabricate data.** Every live collector already degrades to an honest `error` /
  `not_found` / "needs credential" note — never invented rows. The one flagged risk is
  `dark_web_monitor` (PARTIAL: a fixed disclaimer / presence-only label); tighten it so it emits
  real fetch results or an explicit empty, never a placeholder. Ingest failures degrade to a note,
  never to seeded credentials.
- **Never discard data.** A source we spend a request on is used exhaustively: every breach record,
  every field, every page, and the detail endpoint behind each list hit. A bounded view (LLM prompt,
  mobile list) states in-band how much of how much it shows; anything left unused is reported as a
  `collector-truncation-notice` record, not a log line. Run `make audit-sources` (expect 0) after
  any collector change.

This doc is not legal advice. Get counsel before hosting identity data.

### Leak-collector audit (2026-08-10)

Fabrication + exhaustiveness pass over all breach/leak/paste/darkweb collectors
(`breach_*`, `credential_leak`, `dehashed_search`, `dark_web_monitor`,
`pastebin_monitor`, `github_leaks_jp`, `global_ransomlook`,
`cyi_xposedornot_breaches`, `hudson_rock_jp`, `leakix_jp`, `password_checker`,
`global_aleph`, `global_icij`):

- **Exhaustiveness (`make audit-sources`):** 0 findings across the leak set; the
  strict `hp*` gate stays at 0.
- **Fabrication:** none emit invented rows. `SOURCE_REALITY_REPORT.md` was
  **stale** — it still flagged `DARK_WEB_MONITOR`, `DEHASHED_SEARCH`, and
  `PASSWORD_CHECKER` for "names-not-data" halves that had already been removed;
  the report is now corrected (PARTIAL 21 → 19).
- **Fixed this pass:** (1) `dark_web_monitor` Pastebin path counted `/raw/` hits
  and **discarded the paste identities** — now emits one record per distinct
  paste URL (house-rule #2). (2) `dehashed_search` header comment contradicted
  its code ("never sends Basic auth" — it does); corrected.
- **Open, flagged not fixed** (changes an emitted contract — owner's call):
  `password_checker` still writes the **full** SHA-1/SHA-256/MD5 of the
  submitted password into its body JSON, which `breach-check-pipeline.md` §7
  says the served path must not do (emit the 5-char prefix only). This is the
  live OSINT-pivot tool, not the offline `BREACH_INDEX_PASSWORD` path, but it
  persists a plaintext-equivalent digest into `intel_items`.

---

## 8. Phased roadmap

**Phase 1 — Password pivot, fully offline (lowest risk, highest value).**
Ingest Pwned Passwords bulk → `data/breach/password/`. Regenerate `breach-corpus.json`, seed
`breach_meta`. Ship k-anon `GET /api/breach/range/password/{prefix5}`. Verify per
`breach-check-pipeline.md` §8 runbook (synthetic fixtures only). → *Working offline HIBP password
check, zero query-time API.*

**Phase 2 — Identity pivots via licensed feeds.**
Build the feed-ingest adapter (SpyCloud/Enzoic/Constella auth + bulk export → S2→S6). Add
`breach_items`/`breach_fts` bulk-materialize path (throughput plan) if going full-corpus. Extend
`range`/`check` to email/username/phone. → *Email/user/phone presence + which-breach, offline.*

**Phase 3 — Owner-reveal + metadata polish.**
Auth-gated `GET /api/breach/mine` (Supabase identity + ownership verification → `reveal=1`). Wire
XposedOrNot/HIBP metadata into an out-of-band `breach_meta` refresh so every hit has a real title. →
*"See my own leaked password" feature, sensitive-breach gating live.*

**Phase 4 — Scale + monitoring breadth.**
Sorted-binary mmap `.bin` store for billion-row lookup. Harden `dark_web_monitor` (PARTIAL).
Optional: ransomware.live, additional paste monitors, more Tier-D pivot APIs behind keys. →
*Billion-row scale + broad live monitoring, corpus plane still zero-API.*

Every ingest is human-approved and provenance-logged (§7). Sources stay tiered A/B/C/**never-X**.
