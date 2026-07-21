# Breach Ingestion Revamp — Implementation Plan (eager, all datapoints → intel)

_Scope decisions:_
- **No entity-web / relationships.** Datapoints are not linked to each other.
- **Every datapoint is indexed as an intel_item, eagerly, at ingest** — not lazily on
  lookup. The whole corpus becomes searchable intel.
- **Fetch + ingest stay one-shot, admin-triggered, never scheduled.**

Because we're now materializing the *entire* corpus, the binding constraint is **ingest
throughput** — and the dominant cost is the FTS mirror. This plan is built around a bulk
write path and an FTS redesign that make full materialization tractable. That engineering
is §3, the heart of the plan.

---

## 1. Current state (verified)

- `--ingest <source_id> <file>` (`main.c:82`) → `breach_index_ingest` (`breach_index.c:219`),
  **no `db_handle`**. Per-row loop (`:229-279`): normalize (`norm` `:49`) → SHA-1 (`:266`) →
  bloom-dedup per type (`:270`) → AES-256-GCM-encrypt any secret (`enc_secret` `:123`/`:273`) →
  append to prefix-sharded flat files (`:276-277`). Never touches SQLite.
- The shared write path `intel_sink.emit` (`intel.c:96`) does, **per row**: one upsert + a
  **MeCab FTS mirror** (`fts_write` → `fts_segment`, `core/fts.c:61`), in **autocommit**. That is
  three compounding bottlenecks for bulk: MeCab per row, an fsync per row, incremental FTS
  maintenance per row.
- Lookups already emit sanitized intel (`breach_index_svc.c:emit_lookup` `:20`, `reveal=0` `:26`,
  keyid `breach_index_keyid` `:177`). Reveal path `breach_index.c:326`.
- Scheduler guard `scheduler.c:111` skips `interval<=0`; breach services declare `0` (`:71,81,91,99`).
- Admin gate `httpd.c:532-539` (`opgate_check` triad).

---

## 2. Scale — accepted, and sized honestly

Full materialization means the datapoint count *is* the row count:

| Corpus (identity records) | Datapoints → breach rows | Base-table size | FTS index (fast tokenizer) |
|---|---|---|---|
| 10 M | ~40 M | ~4–8 GB | ~2–5 GB |
| 100 M | ~400 M | ~40–80 GB | ~20–50 GB |
| 1 B | ~4 B | ~0.4–0.8 TB | ~0.2–0.5 TB |

This is the deal you're taking: disk grows with the corpus. The plan's job is to make the
**time** acceptable and to keep this volume from degrading the *operational* intel search.
Target: **≥100 k rows/sec** sustained ingest (vs the current path's ~few-k/sec, MeCab-bound).

---

## 3. The throughput redesign (core of this plan)

Five changes, each independently a large multiplier. Together they turn "days" into "hours".

### 3a. Skip the FTS pre-segmentation step; use a lean dedicated FTS index
Correction after reading the code: the existing FTS tables are already `unicode61`, and
`fts_segment` (`fts.c:61`) **already passes Latin text through untouched** — MeCab only runs
when `fts_has_japanese()` is true (`fts.c:79`). So MeCab is *not* the bottleneck for Latin
identifiers; it's simply never called for them. The real per-row costs are the fat
`intel_items` upsert, cJSON building, the uid→rowid FTS map, and autocommit.

→ Give breach data its **own FTS5 index** and bypass the intel path entirely:
- `breach_fts` is an **external-content** FTS5 table (`content='breach_items', content_rowid='id'`)
  with plain `unicode61 remove_diacritics 1` — default unicode61 already splits `user@ntt.co.jp`
  into `user, ntt, co, jp` (searchable by local-part and domain label), no pre-segmentation, no
  MeCab, no per-row cJSON.
- **Optional: `trigram`** tokenizer if substring search (`… LIKE '%kessal%'`) is needed later.
  Bigger index; enable per-deployment.
- Passwords are **never** tokenized as text (hash-only value = NULL, §6).

Combined with §3b–§3d this moves ingest from the intel sink's ~few-k rows/s to a
tokenizer/IO-bound ~100k+ rows/s.

### 3b. Batched transactions + prepared-statement reuse
Replace per-row autocommit with a bulk API — `breach_bulk_begin() / _emit() / _commit()` — that
holds **one transaction per N rows** (start N≈50 k, tune) and **reuses one prepared INSERT**,
binding params directly. No per-row cJSON build; write the lean columns straight (§5). Removes
the fsync-per-row and the parse/serialize overhead.

### 3c. Deferred / bulk FTS build
Don't maintain the FTS index incrementally during the load. Insert all base rows first, then
build the FTS index once at the end: `INSERT INTO breach_fts(breach_fts) VALUES('rebuild')`
(or one big contentless-insert transaction). Bulk build is far cheaper than N incremental
updates, and it lets the base-row insert run at raw speed.

### 3d. Bulk-load PRAGMAs on the ingest connection
Scoped to the ingest job only, restored after: `journal_mode=WAL`, `synchronous=NORMAL`
(or `OFF` for a fully-rebuildable load), `temp_store=MEMORY`, `cache_size=-262144` (256 MB),
`mmap_size` large. Wrap the load so a crash mid-ingest is recoverable (the shard corpus is the
source of truth; a failed materialize is just re-run).

### 3e. Producer/consumer pipeline (optional, later)
SQLite is single-writer, so one writer thread. But normalize+SHA-1+bloom+tokenize can run on
worker threads feeding the writer batches, keeping the writer I/O-bound rather than CPU-bound.
Add only if 3a–3d don't hit target.

---

## 4. Storage — a dedicated `breach_items` table, surfaced as intel

**Do not pour billions of breach rows into the operational `intel_items` + its MeCab FTS.**
That would (1) dilute normal intel search (every query now scans breach rows), (2) force the
MeCab tokenizer on data that shouldn't use it, and (3) couple breach retention/rollback to
operational data.

→ **`breach_items`** (lean, own FTS `breach_fts` with the §3a tokenizer), **surfaced through the
same intel API** as intel items — a typed query / `UNION`/view at the read layer, so from the
client's perspective "every datapoint is an intel item," while physically the two stores stay
independent and each search stays fast. This preserves "datapoints are intel" at the surface
that matters (the API) without sabotaging operational search.

_This is the one deviation from literally reusing `intel_items`. If you specifically want the
same physical table, say so — but then breach rows need their own FTS partition + tokenizer
anyway, so the dedicated table is strictly better. Recommending the dedicated table._

---

## 5. Data model (`breach_items` — lean, no relationships)

Minimal columns (row size drives I/O, which drives ingest time):
- **`keyid` TEXT PRIMARY KEY** — `<type>:<sha1prefix>` (`breach_index_keyid` `:177`), non-reversible,
  idempotent across re-ingest (PK upsert = dedup).
- **`type`** — `email | username | phone | password_hash`.
- **`source_id`** — the breach dataset id.
- **`value`** — cleartext identifier **for email/username/phone only** (needed to be searchable as
  intel — see §6 tradeoff); **NULL for password_hash**.
- **`secret_ref`** — pointer to the AES-GCM shard blob for the reveal path; no plaintext here.
- **`count`** — prevalence (passwords) / occurrence.
- **`breaches_json`** — which breach(es) this keyid appears in.
- **`first_seen`**.
- FTS: `breach_fts(value)` contentless (`content=''`) so the token text isn't stored twice.

Dedup: keep the **bloom filter** for in-ingest dedup (avoids a DB probe per row during the huge
load) **and** the `keyid` PK upsert for cross-run idempotency.

---

## 6. Security / privacy

- **Passwords: hash-only, always.** `type="password_hash"`, `value=NULL`, node identified by SHA-1 /
  keyid. Plaintext secrets stay in the AES-GCM shard (`enc_secret` `:273`), reachable only via the
  existing ownership-verified `reveal=1` path (`breach_index.c:326`) — untouched.
- **Identifier cleartext is now in the DB by design.** To be *searchable as intel*, email/username/
  phone values (and their FTS tokens) must be stored cleartext — there is no way around it if you
  want value search. This is a deliberate policy call that comes with "all datapoints as intel."
  Mitigations: keep the DB file at rest on encrypted storage; gate all breach read APIs behind the
  operator triad; document retention + the one-DELETE rollback (§8).
- **Fail-closed crypto** (`enc_secret` NULL without master key, `:125`) unchanged.

---

## 7. Admin trigger — fetch once, re-launch on demand, never crond

- **Fetch (download)** = standalone one-shot, not a `source_def`: `--fetch <source_id>` CLI +
  `POST /api/admin/breach/fetch`. Stages the dataset to disk; never crond (`scheduler.c:111`).
- **Ingest** threads `db_handle* + intel_sink*`/bulk-writer into `breach_index_ingest`
  (`breach_index.c:219`) as optional params — **NULL preserves today's offline-shard behavior**
  (the `main.c:94` call stays NULL). New `--materialize` opens the DB (`main.c:102` path) and runs
  the §3 bulk path. A `--dry-run` prints the projected row/disk count first.
- **Ingest API** `POST /api/admin/breach/ingest {source_id,path,type,materialize}`, opgated
  (`httpd.c:532-539`), runs as a **background job thread** with `progress_*` reporting — a multi-GB
  ingest must never block the mongoose handler.
- Scheduler stays clean: no new positive-interval source.

---

## 8. Phasing, risks, reversibility

**Phases**
1. **`breach_items` + `breach_fts` schema** (dedicated table, §3a tokenizer) + the bulk writer
   (`_begin/_emit/_commit`, §3b) + bulk PRAGMAs (§3d). Benchmark rows/sec on a real dump.
2. **Deferred FTS build** (§3c) + `--materialize`/`--dry-run` CLI wiring into `breach_index_ingest`
   (NULL-safe). Now the full eager path works end-to-end.
3. **Intel-API surfacing** of `breach_items` as intel items (read-layer view/UNION) + opgated
   `/api/admin/breach/{fetch,ingest}` background jobs.
4. **(If needed)** producer/consumer pipeline (§3e) and/or trigram tokenizer for substring search.

**Risks & mitigations**
- *Ingest too slow* → §3a (no MeCab) + §3b (batching) + §3c (deferred FTS) are the levers; benchmark
  after phase 1 and pull §3e only if short of target.
- *Operational search dilution* → dedicated `breach_items`/`breach_fts` (§4), never the shared index.
- *Disk blowup* → sized in §2, `--dry-run` projects it before committing; retention policy + easy
  rollback.
- *PII at rest* → passwords hash-only; identifier cleartext is the accepted cost of value-search,
  gated + encrypted-at-rest + one-command purge.
- *Crash mid-ingest* → shard corpus is source of truth; `--materialize` is idempotent (bloom + PK
  upsert) and simply re-run.

**Reversibility:** everything lives in `breach_items`/`breach_fts` — rollback = `DROP`/`DELETE` those,
zero impact on operational `intel_items`. The offline shard path is independent throughout.

---

## 9. Not in scope

- No `entity_relationships` / `co_leaked` / `es_*` graph (no web).
- No multi-field record schema (single-identifier lines are fine; each line → one intel row).
- No lazy on-lookup indexing — materialization is eager and total.
