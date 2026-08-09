# Breach-Check Source Catalog (self-hosted / pre-ingested model)

> Companion to [`breach-check-pipeline.md`](./breach-check-pipeline.md). This is the **source
> registry** for the HIBP-style username/password/email/phone lookup. The model is **fully
> self-hosted**: every dataset is acquired and ingested **ahead of time**, and lookups make **zero**
> query-time API calls.
>
> **Scope boundary (read this).** This file catalogs dataset **identities** — what breaches/
> compilations exist, how big, and which identifier types they feed — plus the **lawful bulk
> channels** for obtaining data. It deliberately contains **no** download URLs, magnet/torrent links,
> forum names, or acquisition instructions for illicitly-obtained credential dumps. Ingest only data
> you are lawfully entitled to hold (see §4). Cataloging identity ≠ endorsing acquisition.

---

## 1. The three artifacts

| Artifact | What it is | How to (re)build |
|---|---|---|
| [`breach-corpus.seed.tsv`](./breach-corpus.seed.tsv) | **1,172-row breach identity manifest** (1,171 distinct names; 3 leading `#` comment lines) — the pasted HIBP-style catalog: `name / records / added / breach_date`. The "which breaches exist" list. | committed; hand-editable |
| [`breach-corpus.json`](./breach-corpus.json) | Normalized manifest (integer counts, stable `id` slugs) → seeds the `breach_meta` table so a hit reports *which* breach. **Currently STALE at 1,018 entries** — it was generated before the seed TSV grew to 1,172 rows; regenerate before relying on it. | `node scripts/fetch-breach-corpus.mjs` (offline) |

> Counts here were wrong in every prior revision of this file (they all said
> 1,018, the size of the *generated* JSON, for the *seed TSV* as well). Verify
> rather than copy: `grep -cvE '^\s*(#\|$)' docs/breach-corpus.seed.tsv` for the
> seed, `grep -c '"id"' docs/breach-corpus.json` for the generated manifest.
| `data/breach/<type>/<prefix>.bin` | The actual **hashed index** (built by the ingest pipeline from data you hold). Not in git. | `japanosint --ingest ...` (pipeline §3) |

The provider *ecosystem* (query-time APIs, tooling) is already cataloged in
[`osint-sources-1000.md`](./osint-sources-1000.md) — §7 Breach, §8 Paste, §10 Username, §11 Email,
§12 Phone, §20 Dark Web. Those are **reference only**; they are *not* on the offline lookup path.

---

## 2. Coverage by identifier — what each lookup needs ingested

The feature answers four pivots. Each is fed by different data classes within the corpus:

| Pivot | Fed by | Primary lawful bulk source |
|---|---|---|
| **Password** | any breach exposing passwords/hashes; password-only wordlists | **Pwned Passwords bulk file** (§4.1) — stands up this pivot alone, fully offline |
| **Email** | breaches whose `DataClasses` include email addresses (the vast majority of the corpus) | licensed feed export / lawfully-held dumps → hashed email index |
| **Username** | breaches exposing usernames/handles (forums, gaming, social) | same; usernames indexed as exact-hash |
| **Phone** | breaches exposing phone numbers (telco, delivery, marketing dumps) | same; E.164-normalized then hashed |

`breach_meta.data_classes` (populated from the manifest / `--live` refresh) drives which index a
breach contributes to at ingest time.

---

## 3. Dataset-identity manifest — notable high-volume entries

The full list is in `breach-corpus.seed.tsv` (1,172 data rows). The largest aggregate datasets — the ones
that dominate coverage and are worth prioritizing/deduping first — by record count:

| Dataset | ~Records | Nature (identifier coverage) |
|---|---|---|
| Synthient Credential Stuffing Threat Data | 2.0B | combo/stuffing aggregate (email + password) |
| Collection #1 | 772.9M | combo compilation (email + password) |
| Verifications.io | 763.1M | marketing/enrichment (email + profile) |
| Onliner Spambot | 711.5M | spambot combo (email + password) |
| Data Enrichment (PDL customer) | 622.2M | enrichment (email + phone + profile) |
| Exploit.In | 593.4M | combo compilation (email + password) |
| Anti Public Combo List | 458M | combo list (email + password) |
| River City Media Spam List | 393.4M | spam/enrichment (email + phone) |
| Combolists Posted to Telegram | 361.5M | combo aggregate (email + password) |
| MySpace | 359.4M | breach (email/user + password) |
| ALIEN TXTBASE Stealer Logs | 284.1M | stealer logs (email + password + URL) |
| Not SOCRadar | 282.5M | aggregate (email) |
| Wattpad | 268.8M | breach (email/user + password) |
| NetEase | 234.8M | breach (email/user + password) |
| Deezer | 229M | breach (email + profile) |
| Cit0day | 226.9M | breach index aggregate (email + password) |
| Twitter (200M) | 211.5M | scrape (email/phone + handle) |
| Synthient Stealer Log Threat Data | 183M | stealer logs (email + password) |
| Adult FriendFinder (2016) | 169.7M | breach (email + password) — **sensitive** |
| Dubsmash / Zynga / MyFitnessPal / MyHeritage | 92–173M ea. | breaches (email/user + password) |
| National Public Data | 134M | identity/enrichment (name + phone + SSN-class) |
| Naz.API | 70.8M | stealer/combo aggregate (email + password) |

Stealer-log and combo aggregates overlap heavily — the **Bloom-filter dedup** (pipeline §S5) is what
keeps the index from storing the same hash a billion times. Entries flagged **sensitive** in
`breach_meta` (affair/adult/health sites) are gated per pipeline §6.3.

---

## 4. Lawful bulk acquisition channels (where the data actually comes from)

### 4.1 Pwned Passwords bulk download — the one you should ingest first
HIBP publishes the **complete Pwned Passwords set** as a bulk file: `SHA-1:count` (and an NTLM
variant), hundreds of millions of hashes, **free and licensed for exactly this offline use**. It is
already hashed, so ingest skips S3/S4 and drops straight into `data/breach/password/`. This alone
delivers the entire **password** pivot with no per-lookup API. Refresh periodically; it's versioned.

### 4.2 Licensed commercial feeds (email / username / phone)
For identity pivots, the lawful bulk path is a **contracted feed**, delivered as periodic bulk
exports you ingest like any dump:
- **SpyCloud** — recaptured breach + malware/stealer data (enterprise).
- **Enzoic** — compromised-credential datasets + local/offline matching APIs.
- **Constella / Sptn-class identity-intel** vendors.
These are licensing relationships, not downloads-of-dumps; terms govern retention and redistribution.

### 4.3 Data you are authorized to hold
- **Your own** breach/customer data (you are the controller).
- **CERT / DPA / law-enforcement-provided** notification datasets (e.g. national CERT feeds).
- **Lawfully redistributable** research datasets and password wordlists that contain **no** tied PII
  (e.g. published password-only lists used for strength checking).

### 4.4 What is NOT a source here
Cracking forums, "combo" marketplaces, Telegram leak channels, torrent indexes of stolen dumps, and
resale sites are **out of scope** — this catalog names such datasets' *identities* (so a hit can be
labeled) but provides **no** means of obtaining them. Ingesting stolen-credential compilations you
are not authorized to hold carries serious, jurisdiction-dependent legal liability (unauthorized
access / handling of stolen data / data-protection law). Get counsel before hosting identity data.

---

## 5. Ingest priority (suggested order)

1. **Pwned Passwords bulk** → password pivot online, zero API, lowest legal risk. *(do first)*
2. **Licensed feed export** → email/username/phone pivots at scale.
3. **Manifest** (`breach-corpus.json`) → `breach_meta` so every hit is labeled with a real breach.
4. Additional authorized datasets, largest-first (§3), Bloom-deduped against what's already indexed.

Every ingest is human-approved and provenance-logged per pipeline §S1/§S7.
