# Breach-Check ("Have I Been Pwned"-style) — Dataset Handling Pipeline

> **Renamed 2026-08-09.** This file was called `docs/pipeline.md`, a title that
> promised the collector/intel pipeline and delivered the breach design plan.
> Six places in the tree already cited it as `docs/breach-check-pipeline.md`
> (`native/main.c`, `core/breach_index.h`, `lib/bigfile.h`, `lib/bloom.h`,
> `collectors/sources/breach_index_svc.c`, `docs/breach-sources.md` ×8) — a
> filename that had never existed. The rename makes every one of those
> citations resolve. The real data pipeline is now documented in
> [`pipeline.md`](./pipeline.md).

> Feature goal: given a **username**, **password**, **email**, or **phone number**, tell the
> user which breaches / leaked datasets it appears in — HIBP-style, privacy-preserving,
> never returning plaintext credentials to third parties.
>
> This doc is the **design/plan**. Source catalogs live in
> [`breach-sources.md`](./breach-sources.md) and [`breach-corpus.seed.tsv`](./breach-corpus.seed.tsv).
> The exhaustive provider list is cross-referenced from [`osint-sources-1000.md`](./osint-sources-1000.md)
> (§7 Breach, §8 Paste, §10 Username, §11 Email, §12 Phone, §20 Dark Web).

---

## 1. Context — why this, and what already exists

The backend is a single C binary (`native/bin/japanosint`); every data source implements the
`source_def` ABI in `native/source.h` and self-registers via `REGISTER_SOURCE`. Breach checking
already exists **as live third-party API passthrough**, but there is **no local corpus, no bulk
ingestion, no offline index**. The gap this pipeline fills is the *dataset-handling* half.

**Reuse — already in the repo (do not rebuild):**

| Capability | File | Reuse as |
|---|---|---|
| HIBP **k-anonymity** password range check (SHA-1, sends 5-char prefix only) | `native/collectors/sources/password_checker.c` | The password lookup path + the k-anon HTTP client to mirror for our own range endpoint |
| HIBP email breach lookup (`breachedaccount`) | `native/collectors/sources/breach_checker.c` | Reference only — kept off the offline request path |
| E.164 phone normalize/validate, dial-code→country | `native/collectors/sources/phone_intel.c` (`normalize_phone`, `validate_phone`) | Phone normalization stage |
| Email format/MX/disposable validation | `native/collectors/sources/email_validator.c` | Email normalization stage |
| Entity-type auto-detection (email/user/phone/hash/ip/…) | `native/collectors/sources/dehashed_search.c` (`detect_type`) | Query router (pick the right index) |
| SHA-1/256/MD5 digests (OpenSSL) | `password_checker.c` (`sha1_hash`…) | Hashing stage |
| CSV streaming parser | `native/lib/csv.h` / `csv.c` | Base for the dump reader (needs a large-file streaming variant) |
| Unified result envelope + persistence + FTS5 | `native/source.h` (`intel_item`), `native/core/intel.c`, `schema.sql` | Where lookup results land |
| Portal-status probes (HIBP/DeHashed/Snusbase/LeakCheck/IntelX/BreachDirectory/LeakIX) | `native/collectors/sources/*_breach.c`, `leakix_jp.c` | Optional — provider health only, not on the lookup path |

**Net-new (does not exist):** bulk dump ingestion/ETL, a local hashed credential store, a
password KDF choice, a Bloom/binary-search prefilter, our own k-anonymity range API, a
username index, a phone index, and the corpus metadata catalog.

---

## 2. The one design decision: what we store, and who can see it

Two-tier access, so a user can actually **see their own leaked password** (the whole point) without
building an anonymous plaintext-lookup engine (the WeLeakInfo model — illegal and catastrophic):

- **Identifier → hash.** Emails/usernames/phones are indexed by `SHA-1` only; the anonymous lookup
  path returns **hit / count / which-breach**, never plaintext.
- **Leaked secret → encrypted at rest.** The actual leaked password/credential is stored
  **AES-256-GCM encrypted** (HKDF from `SECRETS_MASTER_KEY`, mirroring `keysapi.c`). It is decrypted
  and returned **only** on an authenticated, **ownership-verified** request for the requester's *own*
  identifier (`reveal=1`). No master key set → secrets are stored as `-` and reveal is impossible
  (fail-closed).

This is the SpyCloud/Constella model: the data subject sees their own exposed credentials; nobody
can anonymously look up someone else's plaintext. `breach_index_lookup(..., reveal=0)` is the only
call the anonymous collectors make; `reveal=1` is reserved for an auth-gated `/api/breach/mine`.

Model = **fully self-hosted / offline**. Every dataset is acquired and ingested **ahead of time**;
lookups touch only our local index and make **zero query-time network calls**.

- **Self-hosted index (the "dataset pipeline")** — we ingest breach *dumps/compilations/DBs* once,
  into our own hashed, deduplicated, prefix-sharded index for fast keyless lookup of
  email/username/phone/password **presence** (+ which breach). This is the whole engine.
- **No live provider dependency at query time.** Third-party APIs (HIBP/DeHashed/Snusbase/…) are
  *not* on the request path. At most they are used **offline, one-off, out-of-band** to help build
  the breach *metadata manifest* (names/dates/counts) — never the credential data, never per-lookup.

**Lawful bulk acquisition (this is where the actual data comes from — see [`breach-sources.md`](./breach-sources.md)):**
the pipeline is source-agnostic and ingests whatever dumps you *lawfully hold*. The genuinely
downloadable-in-bulk source is **Pwned Passwords** (HIBP): the full SHA-1 **+ NTLM + prevalence
count** corpus is a free, licensed bulk file built for exactly this offline model — it stands up the
entire password path with no API. Email/username/phone breach association comes from **licensed
commercial feeds** (SpyCloud/Enzoic-class) or **datasets you are authorized to hold** (your own
breach, CERT/DPA-provided, or lawfully redistributable). This project does **not** source raw
credential dumps from illicit channels.

Per identifier type, what lands in the index:

| Identifier | Normalization | Stored value | Lookup API |
|---|---|---|---|
| **Password** | trim blanks only | `SHA1(pw)` + prevalence count (from Pwned Passwords bulk) | anonymous: count / compromised. Never echoes the password |
| **Email** | lowercase, trim | `SHA1(email)` + `enc(leaked_pw)` | anonymous: which breaches. Owner: decrypts leaked secret |
| **Username** | lowercase, trim | `SHA1(username)` + `enc(leaked_pw)` | anonymous: which breaches. Owner: decrypts leaked secret |
| **Phone** | E.164 (digits + leading `+`) | `SHA1(e164)` + `enc(leaked_pw)` | anonymous: which breaches. Owner: decrypts leaked secret |

On-disk shard record (identity types): `SHA1HEX \t breach_id \t enc_hex|-`; password shards:
`SHA1HEX \t count`. Sharded by the first 2 hex of the SHA-1 (`data/breach/<type>/<xx>.idx`). The
`breach_id` is the ingest source label, so a hit returns *which* breach directly — no join needed
(the `breach-corpus.json` manifest supplies human titles/dates for display). Note: the existing
`password_checker.c` persists full digests in its body JSON — keep that off the served path (§7).

---

## 3. Pipeline stages

```
 acquire ─▶ stage/verify ─▶ parse ─▶ normalize ─▶ hash ─▶ dedup ─▶ shard/index ─▶ publish
   (S0)         (S1)         (S2)       (S3)       (S4)     (S5)       (S6)          (S7)
                                                                                     │
 query:  identifier ─▶ detect type ─▶ normalize ─▶ hash ─▶ shard lookup ─▶ result   (no network)
```

- **S0 Acquire** — obtain the dataset **once, ahead of time**, into staging (see `breach-sources.md`
  for the dataset manifest and the lawful bulk sources). Two acquisition modes: (a) **bulk file** —
  a lawfully-held dump/compilation/DB or the Pwned Passwords bulk hash file, downloaded once; (b)
  **licensed feed export** — a periodic bulk export from a commercial provider under contract. There
  is **no per-lookup fetch**. Each ingest job records provenance (source id, origin, sha256 of the
  file, fetched-at). The breach *metadata manifest* (names/dates/counts, not credentials) comes from
  `breach-corpus.json` built by `scripts/fetch-breach-corpus.mjs` (offline from the committed seed).
- **S1 Stage & verify** — land the raw file in an encrypted, access-logged staging area *outside*
  the served DB. Record file hash, size, row estimate. Quarantine until a human marks the source
  `approved` (legal gate). No auto-ingest of unvetted dumps.
- **S2 Parse** — streaming line reader (net-new; extend `native/lib/csv.c` to a bounded-memory
  `bigfile_reader` that never loads the whole file). Handle the messy real formats: `email:pass`,
  `email:hash`, `user:pass`, CSV/TSV, SQL dumps, JSON-lines stealer logs. Bad lines are counted and
  skipped, never guessed.
- **S3 Normalize** — per §2 table. Reuse `phone_intel.c` / `email_validator.c` / `dehashed_search.c`
  `detect_type`. Emit a typed record `{type, normalized_value, breach_id}`.
- **S4 Hash** — SHA-1 (for HIBP-compatible k-anon) and SHA-256; passwords hashed and the plaintext
  buffer zeroed immediately (`OPENSSL_cleanse`). We store hashes, not plaintext, from here on.
- **S5 Dedup** — a `(type, hash, breach_id)` is unique; a `(type, hash)` may map to many breaches.
  Dedup within a job with an on-disk sort+unique (external merge sort; datasets exceed RAM). A
  Bloom filter (net-new, `native/lib/bloom.c`) gives an O(1) "definitely-not-present" prefilter to
  skip re-inserting the billions of already-known hashes (e.g. RockYou/COMB overlap).
- **S6 Shard & index** — write into the store sharded by the **first 2 hex of the SHA-1**
  (256 buckets), so a lookup touches exactly one shard file. *This doc used to say "first 5 hex
  (65,536×16 buckets)" here and in §4 while §2 said 2 hex — it contradicted itself on the one
  number the on-disk layout depends on. 2 hex is what `shard_path()` in
  `native/core/breach_index.c` actually writes, and §2 was the correct half.* Storage in §4.
- **S7 Publish** — flip the job to `live`, update `breach_meta`, bump the corpus version. Emit an
  audit event (the repo already hash-chains `audit_events`).

**Query path** reuses S3–S4 on the single input, then a shard lookup against the local store only —
no network — returning the standard `intel_item` envelope so the existing SSE/search UI renders it
unchanged.

**Special case — Pwned Passwords bulk file (the password path, fully offline):** the HIBP bulk
download already ships as `SHA1:count` (and an NTLM variant). Ingesting it is S2→S6 with S3/S4
skipped (already hashed). It drops straight into `data/breach/password/<xx>.idx`, giving the
complete offline password k-anonymity feature with zero third-party calls at query time.

---

## 4. Storage

The credential index is large and write-once-read-many, so keep it **out of** `japanmap.db`:

- **As shipped (`native/core/breach_index.c`):** line-oriented text shards at
  `data/breach/<type>/<xx>.idx`, where `<xx>` is the first 2 hex of the SHA-1 (256 files per
  type). Records are `SHA1HEX \t breach_id \t enc_hex|-` for the identity types and
  `SHA1HEX \t count` for passwords. Lookup opens one shard and scans it. `$JO_BREACH_DIR`
  overrides the root.
- **Planned upgrade (NOT built — do not cite this as existing):** a sorted binary store — for each
  SHA-1 prefix a sorted array of `{suffix(35 hex→bytes), breach_bitset}`, binary-searched and
  memory-mapped read-only, at `data/breach/<type>/<prefix>.bin`. This is the HIBP model and is
  what scales to billions of rows with tiny per-query I/O; the text shards above do not.
- **Metadata:** a new `breach_meta` table *in* `japanmap.db` (small): `breach_id, name, title,
  domain, breach_date, added_date, pwn_count, data_classes_json, verified, sensitive, source_id,
  ingested_at`. Seeded offline from `breach-corpus.json` (the committed manifest).
- **Provenance/ops:** reuse the existing `fetch_log`; add an `ingest_job` table
  (`job_id, source_id, file_sha256, rows_in, rows_new, status, staged_at, published_at`).

Rebuilds are per-shard and idempotent; a corrupt shard rebuilds from staging without touching others.

---

## 5. Lookup API surface

Mirror HIBP's privacy model, expose via the existing Mongoose router (`native/core/httpd.c`):

- `GET /api/breach/range/{type}/{prefix5}` — **k-anonymity**. Returns all `suffix:count` (and
  breach-id bitmask) for that 5-hex prefix. Client computes the full hash locally and matches the
  suffix. The server never learns the queried value. `type ∈ {password,email,username,phone}`.
- `POST /api/breach/check {type, value}` — server-side convenience (value hashed in-process, never
  stored/logged); returns `{found, breaches:[...], paste_count}`. Gate behind auth + quota.
- `GET /api/breach/breaches` / `GET /api/breach/breach/{id}` — corpus metadata (public, like HIBP).

All four endpoints read the **local index only** — no third-party calls on the request path. The
existing live-API `source_def`s (`BREACH_CHECKER`, `PASSWORD_CHECKER`, `DEHASHED_SEARCH`, …) are kept
in the tree but are **not** wired into these endpoints; leave them for the LLM-pivot search flow, or
retire them, per your call.

New collectors to register (`.collector="osint"`, `interval=0`, `internal://` url):
`BREACH_INDEX_EMAIL`, `BREACH_INDEX_USERNAME`, `BREACH_INDEX_PHONE`, `BREACH_INDEX_PASSWORD` —
each `run()` normalizes `ctx->entity`, hashes, does the shard lookup, emits `intel_item`s.

---

## 6. Legal / ethical guardrails (non-negotiable, and they shape the code)

1. **Plaintext only to the verified owner.** Identifiers are indexed as hashes; leaked secrets are
   AES-256-GCM encrypted at rest and returned **only** via `reveal=1` after ownership verification.
   The anonymous path (`reveal=0`) never returns plaintext. No `SECRETS_MASTER_KEY` → reveal is
   impossible (fail-closed). There is **no** endpoint that returns plaintext for an arbitrary target.
2. **Human approval gate before ingest** (S1). Datasets stay quarantined until a source is marked
   `approved`; provenance + file hash recorded for every ingest.
3. **Sensitive breaches** (`sensitive=1` in `breach_meta`, e.g. affair/health sites) are never
   listed by public email lookup — HIBP's rule. Only the verified account owner (opt-in / domain
   verification) may see them.
4. **Data-subject rights** — support delete-by-hash and a suppression list.
5. **Access control + audit** — multi-tenant quotas already exist; every check writes an
   `audit_events` row (already hash-chained).
6. **Lawful acquisition only.** Because everything is pre-downloaded and self-hosted, *where the data
   comes from* is the whole legal exposure. Ingest only: the Pwned Passwords bulk file (licensed for
   this), commercial feeds under contract, your own breach data, or datasets you are otherwise
   authorized to hold. Do **not** ingest credential dumps obtained from illicit sources — hosting
   compilations of stolen credentials has serious, jurisdiction-dependent legal weight. `breach-sources.md`
   catalogs dataset *identities* (what exists) and lawful bulk channels; it deliberately provides **no**
   illicit download locations. This doc is not legal advice — get counsel before hosting identity data.

---

## 7. Correctness note for reuse

`password_checker.c` today writes the **full** `sha1`/`sha256`/`md5` of the submitted password into
the emitted `body` JSON (only the *DB key* is limited to the 5-char prefix), so a plaintext-equivalent
hash is persisted in `intel_items`. The new password path must **not** do this: emit only
`{prefix, found, count, breaches}` and never the full digest. Fix or wrap `password_checker.c`
before reusing it in the served path.

---

## 8. Build / verify runbook

**Implemented** (this change): `native/lib/bigfile.{h,c}`, `native/lib/bloom.{h,c}`,
`native/core/breach_index.{h,c}`, `native/collectors/sources/breach_index_svc.c` (the four
`BREACH_INDEX_*` collectors), and `--ingest` in `native/main.c`. New `lib/*.c` and
`collectors/sources/*.c` are picked up by the Makefile wildcards automatically — no Makefile edit.
Must be built on the Linux/macOS toolchain (needs libcurl/openssl/mecab); it does not build on Windows.

```sh
# 0. manifest (offline, no network)
node scripts/fetch-breach-corpus.mjs                      # -> docs/breach-corpus.json

# 1. build
cd native && make                                         # wildcards compile the new files

# 2. synthetic fixture — NEVER a real dump
printf 'alice@example.com:hunter2\nbob@example.com:letmein\n' > /tmp/fix.txt
export JO_BREACH_DIR=/tmp/breach                          # keep the test index out of data/
export SECRETS_MASTER_KEY=$(openssl rand -hex 32)         # enables owner-reveal at rest

# 3. ingest (email pivot)
./bin/japanosint --ingest fixture-breach /tmp/fix.txt --type email
#   -> [ingest] source=fixture-breach type=email rows_in=2 rows_new=2

# 4. anonymous lookup — hit, which breach, NO plaintext
./bin/japanosint --run BREACH_INDEX_EMAIL alice@example.com
#   body: {"found":true,"count":1,"type":"email","breaches":[{"breach":"fixture-breach"}]}
#   (note: no "secret" field — anonymous path is reveal=0)

# 5. password prevalence (Pwned Passwords bulk format: SHA1HEX:count)
printf '%s:42\n' "$(printf 'hunter2' | sha1sum | cut -d' ' -f1 | tr a-z A-Z)" > /tmp/pw.txt
./bin/japanosint --ingest pwned-passwords /tmp/pw.txt --type password
./bin/japanosint --run BREACH_INDEX_PASSWORD hunter2
#   body: {"found":true,"count":1,"type":"password","pwn_count":42,"compromised":true}

# 6. confirm NO plaintext leaked into the main DB, and zero network egress
grep -a hunter2 data/japanmap.db && echo "LEAK!" || echo "clean"
#   run steps 3–5 with egress blocked to prove no outbound calls
```

**Next (not in this change):** the auth-gated `GET /api/breach/mine` endpoint in `native/core/httpd.c`
that, after verifying the caller owns the email (Supabase identity + a verification step), calls
`breach_index_lookup(..., reveal=1)` and returns the decrypted leaked passwords — the "see my own
leaked password" feature. Plus a k-anonymity `GET /api/breach/range/{type}/{prefix5}` for anonymous
checks. Both read the local index only.
