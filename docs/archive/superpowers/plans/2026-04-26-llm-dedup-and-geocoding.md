# LLM-Assisted Station Dedup, Social Geocoding, and Video Geocoding — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **No tests.** Per user direction this plan ships production code only. Implementer subagents must not write `*.test.js` files, must not run `node --test`, and must not add `reset()` / stub-server helpers. Verification is by smoke commands (`node -e ...`) confirming imports, schema, and wiring.

**Goal:** Add an LM Studio-backed enricher that resolves ambiguous station-merge pairs and geocodes ungeocoded social posts, video items, and uncertain cameras — by extracting a Japanese place name with the LLM, then resolving it to coordinates via the existing GSI address-search API.

**Architecture:**
- **Client:** `llmClient.js` — one ~80-line OpenAI-compatible HTTP client targeting LM Studio at `http://localhost:1234/v1/chat/completions`. Returns parsed JSON or `null` on any failure; never throws.
- **Worker:** `llmEnricher.js` — scheduler-tick worker that drains four queues per tick: station dedup pairs, ungeocoded social posts, ungeocoded video items, location-uncertain cameras. Each row's nullable `llm_geocoded_at` column IS the queue (no separate job table, no cache).
- **Storage:** new `social_posts` and `video_items` tables (lat/lon nullable), new `llm_station_merges` pair table, plus two new nullable columns on `cameras`.
- **Clusterer:** `stationClusterer.js` gains a Pass-3 union read from `llm_station_merges`, plus exports `findUncertainStationPairs()` for the worker to consume.
- **Collectors:** `socialMedia.js` and `twitterGeo.js` rewritten to persist into `social_posts` and read geocoded rows back; their FeatureCollection shape is preserved.

**Tech Stack:** Node 20+, ESM, `better-sqlite3`, built-in `fetch`, `node-cron`.

**Spec:** `docs/superpowers/specs/2026-04-26-llm-dedup-and-geocoding-design.md`

---

## File Structure

**New files:**
- `server/src/utils/llmClient.js` — OpenAI-compatible chat client targeting LM Studio.
- `server/src/utils/gsiAddressSearch.js` — reusable GSI place-name → lat/lon helper extracted from `gsiGeocode.js`.
- `server/src/utils/llmEnricher.js` — async write-behind worker; four drain functions.
- `server/src/utils/llmPrompts.js` — prompt-builder functions (one per job type).

**Modified files:**
- `server/src/utils/database.js` — schema additions: `social_posts`, `video_items`, `llm_station_merges`, two columns on `cameras`.
- `server/src/utils/scheduler.js` — register `llmEnricher` cron tick.
- `server/src/utils/stationClusterer.js` — Pass 3 reading `llm_station_merges`; export `findUncertainStationPairs()`.
- `server/src/collectors/socialMedia.js` — persist all fetched articles into `social_posts`; return FeatureCollection from DB.
- `server/src/collectors/twitterGeo.js` — same persistence rewrite, additionally persists ungeocoded Mastodon posts.
- `server/src/collectors/cameraDiscovery.js` — set `properties.location_uncertain = 1` on rows with fuzzy locations (logic-level only).
- `server/src/collectors/gsiGeocode.js` — switch to use the new shared `gsiAddressSearch` helper.
- `docs/collectors.md` — short paragraph documenting the LLM enricher.

---

## Task 1: GSI address-search helper + gsiGeocode refactor

**Files:**
- Create: `server/src/utils/gsiAddressSearch.js`
- Modify: `server/src/collectors/gsiGeocode.js`

**Context:** `server/src/collectors/gsiGeocode.js` currently calls `https://msearch.gsi.go.jp/address-search/AddressSearch?q=<name>` with a hardcoded query (`東京駅`). We need this same API call as a reusable helper that accepts an arbitrary query and returns `{ lat, lon, title } | null`. Both the LLM enricher and the existing `gsiGeocode` collector will use it.

- [ ] **Step 1: Create the helper**

```js
// server/src/utils/gsiAddressSearch.js
const DEFAULT_BASE = 'https://msearch.gsi.go.jp';
const DEFAULT_TIMEOUT_MS = 8000;

/**
 * Resolve a Japanese place-name to coordinates via GSI's address-search.
 * Returns `{ lat, lon, title }` for the top hit, or `null` on miss / failure.
 * Never throws.
 */
export async function gsiAddressSearch(query, opts = {}) {
  if (!query || typeof query !== 'string') return null;
  const baseUrl = opts.baseUrl || DEFAULT_BASE;
  const timeoutMs = opts.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const url = `${baseUrl}/address-search/AddressSearch?q=${encodeURIComponent(query)}`;

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) return null;
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { return null; }
    if (!Array.isArray(data) || data.length === 0) return null;
    const first = data[0];
    const coords = first?.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) return null;
    const lon = Number(coords[0]);
    const lat = Number(coords[1]);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
    return { lat, lon, title: first?.properties?.title ?? null };
  } catch {
    clearTimeout(timer);
    return null;
  }
}
```

- [ ] **Step 2: Replace gsiGeocode collector to use the helper**

```js
// server/src/collectors/gsiGeocode.js
import { gsiAddressSearch } from '../utils/gsiAddressSearch.js';

const PROBE_QUERY = '東京駅';

export default async function collectGsiGeocode() {
  const hit = await gsiAddressSearch(PROBE_QUERY);
  let features;
  let source;
  if (hit) {
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [hit.lon, hit.lat] },
      properties: { title: hit.title, source: 'gsi_geocode' },
    }];
    source = 'live';
  } else {
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.767, 35.681] },
      properties: { title: '東京駅 (seed)', source: 'gsi_seed' },
    }];
    source = 'seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'GSI address-search geocoder probe',
    },
    metadata: {},
  };
}
```

- [ ] **Step 3: Smoke check the helper**

Run: `cd server && node -e "import('./src/utils/gsiAddressSearch.js').then(async m => { console.log(await m.gsiAddressSearch('東京駅')); })"`
Expected: prints `{ lat: 35.6..., lon: 139.7..., title: '...' }` if GSI is reachable, or `null` otherwise. No exceptions.

- [ ] **Step 4: Smoke check the collector still works**

Run: `cd server && node -e "import('./src/collectors/gsiGeocode.js').then(async m => { const r = await m.default(); console.log(r._meta.source, r.features.length); })"`
Expected: prints `live 1` or `seed 1`. No exceptions.

- [ ] **Step 5: Commit**

```bash
git add server/src/utils/gsiAddressSearch.js server/src/collectors/gsiGeocode.js
git commit -m "feat(server): extract reusable gsiAddressSearch helper"
```

---

## Task 2: LM Studio chat client

**Files:**
- Create: `server/src/utils/llmClient.js`

**Context:** `chat({ messages, jsonSchema, timeoutMs, baseUrl, model })` posts an OpenAI-compatible chat completion to LM Studio and returns the parsed JSON object the model produced (parsed from `choices[0].message.content`). Returns `null` on any failure (timeout, non-2xx, malformed JSON, schema-violating JSON). Never throws past the caller.

- [ ] **Step 1: Write the implementation**

```js
// server/src/utils/llmClient.js
const DEFAULT_BASE = 'http://localhost:1234';
const DEFAULT_MODEL = 'local-model';
const DEFAULT_TIMEOUT_MS = 30_000;

/**
 * OpenAI-compatible chat completion against LM Studio (or any compatible
 * server). Returns the parsed JSON object the model produced, or null on
 * any failure. Never throws past the caller.
 *
 * @param {object} args
 * @param {Array}  args.messages     OpenAI-style messages array. Vision callers
 *                                   may pass an array `content` with `text` and
 *                                   `image_url` parts; the client passes it through.
 * @param {object} args.jsonSchema   JSON Schema enforced via response_format.
 * @param {number} [args.timeoutMs]  Request timeout, default 30s.
 * @param {string} [args.baseUrl]    Override LM Studio URL (env or default otherwise).
 * @param {string} [args.model]      Override model id (env or default otherwise).
 */
export async function chat({ messages, jsonSchema, timeoutMs, baseUrl, model }) {
  const url = `${baseUrl || process.env.LLM_BASE_URL || DEFAULT_BASE}/v1/chat/completions`;
  const body = {
    model: model || process.env.LLM_MODEL || DEFAULT_MODEL,
    messages,
    temperature: 0.1,
    response_format: {
      type: 'json_schema',
      json_schema: { name: 'response', strict: true, schema: jsonSchema },
    },
  };

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs ?? DEFAULT_TIMEOUT_MS);
  try {
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(body),
      signal: controller.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const envelope = await res.json().catch(() => null);
    const content = envelope?.choices?.[0]?.message?.content;
    if (typeof content !== 'string' || content.length === 0) return null;
    try {
      return JSON.parse(content);
    } catch {
      return null;
    }
  } catch {
    clearTimeout(timer);
    return null;
  }
}
```

- [ ] **Step 2: Smoke check (no LM Studio required — verifies fail-closed)**

Run: `cd server && node -e "import('./src/utils/llmClient.js').then(async m => { const r = await m.chat({ messages: [{role:'user',content:'hi'}], jsonSchema: {type:'object'}, timeoutMs: 1000, baseUrl: 'http://127.0.0.1:1' }); console.log('result:', r); })"`
Expected: prints `result: null`. No exceptions.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/llmClient.js
git commit -m "feat(server): add LM Studio OpenAI-compatible chat client"
```

---

## Task 3: Schema migrations — new tables and columns

**Files:**
- Modify: `server/src/utils/database.js`

**Context:** Add `social_posts`, `video_items`, `llm_station_merges` tables and two columns on `cameras`. New tables go inside the `db.exec(...)` block that already holds all `CREATE TABLE IF NOT EXISTS` statements. The two new `cameras` columns must be added through the existing column-migration path because `cameras` already exists in deployed DBs — `CREATE TABLE IF NOT EXISTS` won't add them.

- [ ] **Step 1: Inspect the existing column-migration helper**

Run: `grep -n "ALTER TABLE\|station_line_dots" server/src/utils/database.js`
Expected: a section near the bottom of the file that adds columns conditionally (the existing `station_line_dots` migration block). Use the same pattern.

- [ ] **Step 2: Add the new `CREATE TABLE` statements**

Inside the `db.exec(...)` template literal in `database.js`, after the existing `collector_cache` table, append:

```sql
  -- Persisted social-media posts. Both geocoded and ungeocoded; ungeocoded
  -- rows (lat IS NULL) are picked up by llmEnricher and resolved via LLM
  -- → GSI address search. llm_geocoded_at marks the row as decided so the
  -- worker doesn't loop on it; clear that column to re-process.
  CREATE TABLE IF NOT EXISTS social_posts (
    post_uid          TEXT PRIMARY KEY,
    platform          TEXT NOT NULL,
    author            TEXT,
    text              TEXT,
    title             TEXT,
    url               TEXT,
    media_urls        TEXT,
    language          TEXT,
    fetched_at        TEXT NOT NULL DEFAULT (datetime('now')),
    posted_at         TEXT,
    lat               REAL,
    lon               REAL,
    geo_source        TEXT,
    llm_place_name    TEXT,
    llm_geocoded_at   TEXT,
    llm_failure       TEXT,
    properties        TEXT NOT NULL DEFAULT '{}'
  );
  CREATE INDEX IF NOT EXISTS idx_social_posts_geo
    ON social_posts(lat, lon);
  CREATE INDEX IF NOT EXISTS idx_social_posts_pending
    ON social_posts(llm_geocoded_at) WHERE llm_geocoded_at IS NULL;

  -- Persisted video records (YouTube / Niconico / etc.). Same enrichment
  -- shape as social_posts. No collector populates this table in this PR;
  -- the schema lands so the enricher's drain function has somewhere to
  -- read from when a video collector arrives.
  CREATE TABLE IF NOT EXISTS video_items (
    video_uid         TEXT PRIMARY KEY,
    platform          TEXT NOT NULL,
    channel           TEXT,
    title             TEXT,
    description       TEXT,
    thumbnail_url     TEXT,
    url               TEXT,
    language          TEXT,
    published_at      TEXT,
    fetched_at        TEXT NOT NULL DEFAULT (datetime('now')),
    lat               REAL,
    lon               REAL,
    geo_source        TEXT,
    llm_place_name    TEXT,
    llm_geocoded_at   TEXT,
    llm_failure       TEXT,
    properties        TEXT NOT NULL DEFAULT '{}'
  );
  CREATE INDEX IF NOT EXISTS idx_video_items_geo
    ON video_items(lat, lon);
  CREATE INDEX IF NOT EXISTS idx_video_items_pending
    ON video_items(llm_geocoded_at) WHERE llm_geocoded_at IS NULL;

  -- Pair-level "the LLM said these two transport_stations are/are-not the
  -- same". stationClusterer reads this in Pass 3 and unions any pair with
  -- same=1 AND confidence >= 0.7. Pair ordering: uid_a < uid_b lexically.
  CREATE TABLE IF NOT EXISTS llm_station_merges (
    uid_a       TEXT NOT NULL,
    uid_b       TEXT NOT NULL,
    same        INTEGER NOT NULL,
    confidence  REAL NOT NULL,
    reason      TEXT,
    decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (uid_a, uid_b)
  );
```

- [ ] **Step 3: Add the cameras column-add migration**

Find the existing post-`db.exec` migration block in `database.js` that uses `PRAGMA table_info(...)` to add columns (the `station_line_dots` migration). Add a sibling block immediately after it:

```js
{
  // cameras.llm_place_name / cameras.llm_geocoded_at — LLM-resolved location.
  // Existing rows have NOT NULL lat/lon already; these columns let the
  // enricher overwrite uncertain coords once the LLM has spoken.
  const cols = db.prepare('PRAGMA table_info(cameras)').all().map((c) => c.name);
  if (!cols.includes('llm_place_name')) {
    db.exec('ALTER TABLE cameras ADD COLUMN llm_place_name TEXT');
  }
  if (!cols.includes('llm_geocoded_at')) {
    db.exec('ALTER TABLE cameras ADD COLUMN llm_geocoded_at TEXT');
  }
}
```

- [ ] **Step 4: Smoke-test the schema**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => { const r = m.default.prepare('SELECT name FROM sqlite_master WHERE type=\"table\"').all(); console.log(r.map(x => x.name).sort().join(' ')); })"`
Expected: prints a table list that includes `llm_station_merges social_posts video_items`. No errors.

- [ ] **Step 5: Verify cameras has the new columns**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => { const r = m.default.prepare('PRAGMA table_info(cameras)').all(); console.log(r.map(c => c.name).join(' ')); })"`
Expected: includes `llm_place_name llm_geocoded_at`.

- [ ] **Step 6: Commit**

```bash
git add server/src/utils/database.js
git commit -m "feat(db): add social_posts, video_items, llm_station_merges; cameras llm columns"
```

---

## Task 4: Prompt builders

**Files:**
- Create: `server/src/utils/llmPrompts.js`

**Context:** Each enrichment job has a prompt-building function. Three builders: `buildDedupPairPrompt`, `buildSocialGeocodePrompt`, `buildVideoGeocodePrompt`. All return `{ messages, jsonSchema }`. Vision: `buildSocialGeocodePrompt` and `buildVideoGeocodePrompt` accept `imageUrls` / `thumbnailUrl`; when non-empty AND `vision: true`, they append `image_url` content parts. Cap social images at 2; video uses one thumbnail.

- [ ] **Step 1: Create the file**

```js
// server/src/utils/llmPrompts.js
const MAX_TEXT_CHARS = 500;
const MAX_IMAGES = 2;

const PLACE_SCHEMA = {
  type: 'object',
  properties: {
    place:      { type: ['string', 'null'], maxLength: 100 },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
  },
  required: ['place', 'confidence'],
};

const DEDUP_SCHEMA = {
  type: 'object',
  properties: {
    same_station: { type: 'boolean' },
    confidence:   { type: 'number', minimum: 0, maximum: 1 },
    reason:       { type: 'string', maxLength: 200 },
  },
  required: ['same_station', 'confidence'],
};

function clip(s, n = MAX_TEXT_CHARS) {
  if (!s) return '';
  return s.length <= n ? s : s.slice(0, n) + '…';
}

export function buildDedupPairPrompt(p) {
  const system =
    'You are a Japanese rail station identity matcher. Given two station ' +
    'records that are within 150 metres of each other, decide whether they ' +
    'refer to the same physical station/interchange. Two records describe ' +
    'the same station if a passenger could transfer between them without ' +
    'leaving a paid area or by walking through a connected concourse. ' +
    'Different stations on overlapping platforms are NOT the same station.';
  const user =
    `Station A:\n` +
    `  name: ${JSON.stringify(p.name_a ?? '')}\n` +
    `  name_ja: ${JSON.stringify(p.name_ja_a ?? '')}\n` +
    `  operator: ${JSON.stringify(p.operator_a ?? '')}\n` +
    `  line: ${JSON.stringify(p.line_a ?? '')}\n` +
    `  mode: ${p.mode_a ?? ''}\n\n` +
    `Station B:\n` +
    `  name: ${JSON.stringify(p.name_b ?? '')}\n` +
    `  name_ja: ${JSON.stringify(p.name_ja_b ?? '')}\n` +
    `  operator: ${JSON.stringify(p.operator_b ?? '')}\n` +
    `  line: ${JSON.stringify(p.line_b ?? '')}\n` +
    `  mode: ${p.mode_b ?? ''}\n\n` +
    `Distance: ${Math.round(p.dist_m ?? 0)} m`;
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: user },
    ],
    jsonSchema: DEDUP_SCHEMA,
  };
}

export function buildSocialGeocodePrompt(p) {
  const system =
    'You extract Japanese place names from social media posts. Return the ' +
    'single most specific Japanese place mentioned in the post — a ' +
    'neighbourhood, station, landmark, or address. Prefer the place where ' +
    'the author is, not places merely mentioned in conversation. If no ' +
    'place is mentioned, return null.';
  const userText =
    `Platform: ${p.platform}\n` +
    `Author: ${p.author ?? ''}\n` +
    `Title: ${clip(p.title) || '(none)'}\n` +
    `Text: ${clip(p.text) || '(none)'}`;
  const images = (p.vision && Array.isArray(p.imageUrls))
    ? p.imageUrls.slice(0, MAX_IMAGES).map((url) => ({ type: 'image_url', image_url: { url } }))
    : [];
  const userContent = images.length === 0
    ? userText
    : [{ type: 'text', text: userText }, ...images];
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: userContent },
    ],
    jsonSchema: PLACE_SCHEMA,
  };
}

export function buildVideoGeocodePrompt(p) {
  const system =
    'You extract Japanese place names from video metadata. Return the ' +
    'single most specific Japanese place where the video was filmed or ' +
    'that the video is about — a neighbourhood, station, landmark, or ' +
    'address. If no place can be inferred, return null.';
  const userText =
    `Platform: ${p.platform}\n` +
    `Channel: ${p.channel ?? ''}\n` +
    `Title: ${clip(p.title) || '(none)'}\n` +
    `Description: ${clip(p.description) || '(none)'}`;
  const images = (p.vision && p.thumbnailUrl)
    ? [{ type: 'image_url', image_url: { url: p.thumbnailUrl } }]
    : [];
  const userContent = images.length === 0
    ? userText
    : [{ type: 'text', text: userText }, ...images];
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: userContent },
    ],
    jsonSchema: PLACE_SCHEMA,
  };
}
```

- [ ] **Step 2: Smoke check**

Run: `cd server && node -e "import('./src/utils/llmPrompts.js').then(m => { const x = m.buildDedupPairPrompt({uid_a:'A',uid_b:'B',name_a:'霞ケ関',name_b:'Kasumigaseki',name_ja_a:'霞ケ関',name_ja_b:'霞ヶ関',operator_a:'X',operator_b:'X',line_a:'M',line_b:'H',mode_a:'subway',mode_b:'subway',dist_m:38}); console.log('messages:', x.messages.length, 'has dist:', x.messages[1].content.includes('38')); })"`
Expected: prints `messages: 2 has dist: true`. No exceptions.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/llmPrompts.js
git commit -m "feat(server): prompt builders for dedup, social, video LLM jobs"
```

---

## Task 5: Expose `findUncertainStationPairs` from the clusterer

**Files:**
- Modify: `server/src/utils/stationClusterer.js`

**Context:** The worker needs the list of uncertain pairs to ask the LLM about. Reuse the cell-bucketed neighbour scan from Pass 2. Cap at 500 to keep the worker bounded.

- [ ] **Step 1: Append the export at the bottom of the file**

Append after `getAllLineDotFeatures`:

```js
const MAX_UNCERTAIN_PAIRS = 500;

/**
 * Return up to MAX_UNCERTAIN_PAIRS station pairs that the existing
 * Pass-2 (fingerprint + Levenshtein ≤ 2) clusterer would NOT merge,
 * but which are within 150 m of each other and worth asking an LLM
 * about. Used by llmEnricher.
 *
 * Pair ordering: uid_a < uid_b lexically, so each pair appears once.
 */
export function findUncertainStationPairs() {
  const stations = loadStations();
  if (stations.length === 0) return [];

  const cells = new Map();
  for (let i = 0; i < stations.length; i++) {
    const k = cellKey(stations[i].lon, stations[i].lat);
    let bucket = cells.get(k);
    if (!bucket) { bucket = []; cells.set(k, bucket); }
    bucket.push(i);
  }

  const radiusSq = SPATIAL_RADIUS_M * SPATIAL_RADIUS_M;
  const out = [];
  for (const [key, bucket] of cells) {
    const [cx, cy] = key.split(':').map(Number);
    const candidates = [];
    for (let dx = -1; dx <= 1; dx++) {
      for (let dy = -1; dy <= 1; dy++) {
        const neighbour = cells.get(`${cx + dx}:${cy + dy}`);
        if (neighbour) for (const i of neighbour) candidates.push(i);
      }
    }
    for (const i of bucket) {
      for (const j of candidates) {
        if (j <= i) continue;
        const a = stations[i];
        const b = stations[j];
        if (distSqM(a.lon, a.lat, b.lon, b.lat) > radiusSq) continue;
        const sameFp = a.fingerprint && b.fingerprint && a.fingerprint === b.fingerprint;
        const len = Math.min(a.fingerprint?.length || 0, b.fingerprint?.length || 0);
        const closeFp = len >= 3 && a.fingerprint && b.fingerprint
          && levenshtein(a.fingerprint, b.fingerprint) <= LEVENSHTEIN_THRESHOLD;
        const operatorsDiffer = a.operator && b.operator && a.operator !== b.operator;
        // "Interesting" = Pass 2 would NOT merge, but they're spatially close.
        // Either fingerprint+Levenshtein both miss, OR the names match but the
        // operators differ (cross-operator station that wikidata didn't link).
        const wouldPass2Merge = sameFp || closeFp;
        const interesting = !wouldPass2Merge || (sameFp && operatorsDiffer);
        if (!interesting) continue;
        const [pa, pb] = a.uid < b.uid ? [a, b] : [b, a];
        out.push({
          uid_a: pa.uid,  name_a: pa.name,  name_ja_a: pa.name_ja,  operator_a: pa.operator,  line_a: pa.line_name, mode_a: pa.mode, lat_a: pa.lat, lon_a: pa.lon,
          uid_b: pb.uid,  name_b: pb.name,  name_ja_b: pb.name_ja,  operator_b: pb.operator,  line_b: pb.line_name, mode_b: pb.mode, lat_b: pb.lat, lon_b: pb.lon,
          dist_m: Math.sqrt(distSqM(a.lon, a.lat, b.lon, b.lat)),
        });
        if (out.length >= MAX_UNCERTAIN_PAIRS) return out;
      }
    }
  }
  return out;
}
```

- [ ] **Step 2: Smoke check**

Run: `cd server && node -e "import('./src/utils/stationClusterer.js').then(m => { console.log('typeof:', typeof m.findUncertainStationPairs); const p = m.findUncertainStationPairs(); console.log('pairs:', p.length); if (p.length) console.log('sample:', p[0]); })"`
Expected: prints `typeof: function pairs: <N>`. If the DB has a populated `transport_stations` table, N is some non-negative integer; otherwise 0.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/stationClusterer.js
git commit -m "feat(server): export findUncertainStationPairs from stationClusterer"
```

---

## Task 6: Clusterer Pass 3 — read llm_station_merges

**Files:**
- Modify: `server/src/utils/stationClusterer.js`

**Context:** After Pass 1 (wikidata) and Pass 2 (fingerprint), add Pass 3 unioning any pair the LLM has confidently labelled `same=1, confidence>=0.7`.

- [ ] **Step 1: Modify the `cluster()` function**

In `stationClusterer.js`, edit `function cluster(stations)` body. After the Pass 2 nested loops and *before* the "Collect groups" comment, insert:

```js
  // Pass 3: LLM-decided merges. The worker may have written rows into
  // llm_station_merges since the previous run. We trust same=1 with
  // confidence >= 0.7; lower confidences are ignored (the row just
  // documents what the LLM thought, doesn't force a merge).
  const indexByUid = new Map();
  for (let i = 0; i < stations.length; i++) indexByUid.set(stations[i].uid, i);
  const merges = db.prepare(`
    SELECT uid_a, uid_b FROM llm_station_merges
    WHERE same = 1 AND confidence >= 0.7
  `).all();
  for (const { uid_a, uid_b } of merges) {
    const i = indexByUid.get(uid_a);
    const j = indexByUid.get(uid_b);
    if (i != null && j != null) dsu.union(i, j);
  }
```

- [ ] **Step 2: Smoke-test by re-running the clusterer**

Run: `cd server && node -e "import('./src/utils/stationClusterer.js').then(m => console.log(m.runStationClusterer()))"`
Expected: prints `{ stations: <N>, clusters: <M>, merges: <K>, dots: <D> }` with no errors. With an empty `llm_station_merges`, output should match the previous run.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/stationClusterer.js
git commit -m "feat(server): clusterer Pass 3 honours llm_station_merges"
```

---

## Task 7: llmEnricher — the worker

**Files:**
- Create: `server/src/utils/llmEnricher.js`

**Context:** Four drain functions (`enrichStationDedup`, `enrichSocialGeocode`, `enrichVideoGeocode`, `enrichCameras`) plus a top-level `runLlmEnricher` that calls them in series. Each row that gets a verdict (success or failure) gets `llm_geocoded_at = datetime('now')` written so the worker doesn't loop on it.

Failure sentinels in `llm_failure`:
- `__bad_json__` — LLM call returned null or no parseable place.
- `__no_match__` — LLM said no place found OR confidence below gate.
- `__gsi_miss__` — GSI returned no result for the LLM-extracted name.

- [ ] **Step 1: Create the file**

```js
// server/src/utils/llmEnricher.js
import db from './database.js';
import { chat as defaultChat } from './llmClient.js';
import { gsiAddressSearch as defaultGsi } from './gsiAddressSearch.js';
import { findUncertainStationPairs as defaultPairs } from './stationClusterer.js';
import {
  buildDedupPairPrompt,
  buildSocialGeocodePrompt,
  buildVideoGeocodePrompt,
} from './llmPrompts.js';

const DEFAULT_BATCH = Number(process.env.LLM_BATCH_SIZE || 50);
const VISION = process.env.LLM_VISION === 'true';
const TIMEOUT_MS = Number(process.env.LLM_TIMEOUT_MS || 30_000);
const PLACE_CONFIDENCE_GATE = 0.5;
// Dedup pairs are recorded regardless of confidence; the clusterer
// separately gates on >= 0.7 when deciding whether to merge.

const stmtInsertMerge = db.prepare(`
  INSERT OR REPLACE INTO llm_station_merges
    (uid_a, uid_b, same, confidence, reason, decided_at)
  VALUES (?, ?, ?, ?, ?, datetime('now'))
`);

const stmtPairExists = db.prepare(`
  SELECT 1 FROM llm_station_merges WHERE uid_a = ? AND uid_b = ?
`);

export async function enrichStationDedup(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const pairsProvider = opts.pairsProvider || defaultPairs;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;

  let decided = 0;
  const pairs = pairsProvider();
  for (const p of pairs) {
    if (decided >= batchSize) break;
    if (stmtPairExists.get(p.uid_a, p.uid_b)) continue;
    const { messages, jsonSchema } = buildDedupPairPrompt(p);
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    if (!out || typeof out.same_station !== 'boolean' || typeof out.confidence !== 'number') continue;
    stmtInsertMerge.run(
      p.uid_a, p.uid_b,
      out.same_station ? 1 : 0,
      out.confidence,
      typeof out.reason === 'string' ? out.reason.slice(0, 200) : null,
    );
    decided++;
  }
  return { decided };
}

const stmtPendingSocial = db.prepare(`
  SELECT post_uid, platform, author, text, title, media_urls
  FROM social_posts
  WHERE llm_geocoded_at IS NULL
    AND (text IS NOT NULL OR title IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

const stmtUpdateSocialOk = db.prepare(`
  UPDATE social_posts
  SET lat = ?, lon = ?, geo_source = 'llm_gsi',
      llm_place_name = ?, llm_geocoded_at = datetime('now'), llm_failure = NULL
  WHERE post_uid = ?
`);

const stmtUpdateSocialFail = db.prepare(`
  UPDATE social_posts
  SET llm_geocoded_at = datetime('now'), llm_failure = ?, llm_place_name = ?
  WHERE post_uid = ?
`);

export async function enrichSocialGeocode(opts = {}) {
  return drainTextRows({
    rowsStmt: stmtPendingSocial,
    buildPrompt: (row) => buildSocialGeocodePrompt({
      platform: row.platform, author: row.author, text: row.text, title: row.title,
      imageUrls: parseJsonArray(row.media_urls), vision: VISION,
    }),
    onOk: (row, place, hit) => stmtUpdateSocialOk.run(hit.lat, hit.lon, place, row.post_uid),
    onFail: (row, sentinel, place) => stmtUpdateSocialFail.run(sentinel, place, row.post_uid),
    ...opts,
  });
}

const stmtPendingVideo = db.prepare(`
  SELECT video_uid, platform, channel, title, description, thumbnail_url
  FROM video_items
  WHERE llm_geocoded_at IS NULL
    AND (title IS NOT NULL OR description IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

const stmtUpdateVideoOk = db.prepare(`
  UPDATE video_items
  SET lat = ?, lon = ?, geo_source = 'llm_gsi',
      llm_place_name = ?, llm_geocoded_at = datetime('now'), llm_failure = NULL
  WHERE video_uid = ?
`);

const stmtUpdateVideoFail = db.prepare(`
  UPDATE video_items
  SET llm_geocoded_at = datetime('now'), llm_failure = ?, llm_place_name = ?
  WHERE video_uid = ?
`);

export async function enrichVideoGeocode(opts = {}) {
  return drainTextRows({
    rowsStmt: stmtPendingVideo,
    buildPrompt: (row) => buildVideoGeocodePrompt({
      platform: row.platform, channel: row.channel, title: row.title,
      description: row.description, thumbnailUrl: row.thumbnail_url, vision: VISION,
    }),
    onOk: (row, place, hit) => stmtUpdateVideoOk.run(hit.lat, hit.lon, place, row.video_uid),
    onFail: (row, sentinel, place) => stmtUpdateVideoFail.run(sentinel, place, row.video_uid),
    ...opts,
  });
}

const stmtPendingCameras = db.prepare(`
  SELECT camera_uid, name, lat, lon, properties
  FROM cameras
  WHERE llm_geocoded_at IS NULL
    AND json_extract(properties, '$.location_uncertain') = 1
  LIMIT ?
`);

const stmtUpdateCameraOk = db.prepare(`
  UPDATE cameras
  SET lat = ?, lon = ?, llm_place_name = ?,
      llm_geocoded_at = datetime('now'),
      properties = ?
  WHERE camera_uid = ?
`);

const stmtUpdateCameraFail = db.prepare(`
  UPDATE cameras
  SET llm_geocoded_at = datetime('now'), llm_place_name = ?
  WHERE camera_uid = ?
`);

export async function enrichCameras(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const gsiSearch = opts.gsiSearch || defaultGsi;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;

  const rows = stmtPendingCameras.all(batchSize);
  let geocoded = 0;
  for (const row of rows) {
    const { messages, jsonSchema } = buildSocialGeocodePrompt({
      platform: 'camera',
      author: '',
      text: row.name,
      title: null,
      imageUrls: [],
      vision: false,
    });
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    if (!out || typeof out.place === 'undefined') {
      stmtUpdateCameraFail.run(null, row.camera_uid);
      continue;
    }
    if (!out.place) {
      stmtUpdateCameraFail.run(null, row.camera_uid);
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      stmtUpdateCameraFail.run(out.place, row.camera_uid);
      continue;
    }
    const hit = await gsiSearch(out.place);
    if (!hit) {
      stmtUpdateCameraFail.run(out.place, row.camera_uid);
      continue;
    }
    const props = safeJson(row.properties);
    props.original_lat = row.lat;
    props.original_lon = row.lon;
    stmtUpdateCameraOk.run(hit.lat, hit.lon, out.place, JSON.stringify(props), row.camera_uid);
    geocoded++;
  }
  return { geocoded };
}

async function drainTextRows({ rowsStmt, buildPrompt, onOk, onFail, llmChat, gsiSearch, batchSize }) {
  const _llmChat = llmChat || defaultChat;
  const _gsiSearch = gsiSearch || defaultGsi;
  const _batchSize = batchSize ?? DEFAULT_BATCH;
  const rows = rowsStmt.all(_batchSize);
  let geocoded = 0;
  for (const row of rows) {
    const { messages, jsonSchema } = buildPrompt(row);
    const out = await _llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    if (!out || typeof out.place === 'undefined') {
      onFail(row, '__bad_json__', null);
      continue;
    }
    if (out.place === null) {
      onFail(row, '__no_match__', null);
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      onFail(row, '__no_match__', out.place);
      continue;
    }
    const hit = await _gsiSearch(out.place);
    if (!hit) {
      onFail(row, '__gsi_miss__', out.place);
      continue;
    }
    onOk(row, out.place, hit);
    geocoded++;
  }
  return { geocoded };
}

function parseJsonArray(s) {
  if (!s) return [];
  try { const v = JSON.parse(s); return Array.isArray(v) ? v : []; } catch { return []; }
}

function safeJson(s) {
  try { return JSON.parse(s); } catch { return {}; }
}

export async function runLlmEnricher() {
  if (process.env.LLM_ENABLED !== 'true') return { skipped: true };
  const out = { skipped: false };
  for (const [name, fn] of [
    ['stationDedup', enrichStationDedup],
    ['social', enrichSocialGeocode],
    ['video', enrichVideoGeocode],
    ['cameras', enrichCameras],
  ]) {
    try {
      out[name] = await fn();
    } catch (err) {
      console.warn(`[llmEnricher] ${name} failed:`, err?.message);
      out[name] = { error: err?.message || String(err) };
    }
  }
  return out;
}
```

- [ ] **Step 2: Smoke check (no LM Studio required — verifies short-circuit)**

Run: `cd server && node -e "import('./src/utils/llmEnricher.js').then(async m => console.log(await m.runLlmEnricher()))"`
Expected: prints `{ skipped: true }` (because `LLM_ENABLED` is unset). No exceptions.

- [ ] **Step 3: Smoke check the queries don't error when run with the flag on but no LM Studio**

Run: `cd server && LLM_ENABLED=true LLM_BATCH_SIZE=2 node -e "import('./src/utils/llmEnricher.js').then(async m => console.log(JSON.stringify(await m.runLlmEnricher())))"` (timeout: 60s)

Note: this attempts real HTTP to `http://localhost:1234`. If LM Studio isn't running, every LLM call returns `null`, every pending row gets `__bad_json__`, and the function returns finite counts. Acceptable shape: `{ skipped: false, stationDedup: { decided: 0 }, social: { geocoded: 0 }, video: { geocoded: 0 }, cameras: { geocoded: 0 } }`. No exceptions.

⚠️ This may write `__bad_json__` sentinels into real social_posts rows if any are pending. That's the *correct* behaviour, but if you want to avoid touching real data during smoke, skip this step and rely on Step 2 only. **Recommendation: skip Step 3 unless explicitly requested.**

- [ ] **Step 4: Commit**

```bash
git add server/src/utils/llmEnricher.js
git commit -m "feat(server): llmEnricher async write-behind worker"
```

---

## Task 8: Wire the enricher into the scheduler

**Files:**
- Modify: `server/src/utils/scheduler.js`

**Context:** Add a cron tick calling `runLlmEnricher`. Function short-circuits when `LLM_ENABLED!=true`, so registering unconditionally is safe. Cron `*/5 * * * *`, placed after the GTFS-RT catalogue refresh.

- [ ] **Step 1: Add the import**

At the top of `scheduler.js` with the other runner imports:

```js
import { runLlmEnricher } from './llmEnricher.js';
```

- [ ] **Step 2: Add the schedule registration**

After the `cron.schedule('45 * * * *', …)` block (the GTFS-RT catalogue refresh), append:

```js
  // 8. LLM enricher — every 5 minutes. Short-circuits when LLM_ENABLED!=true,
  //    so registering unconditionally is safe even on machines without
  //    LM Studio. Drains four queues per tick:
  //      - station dedup pairs → llm_station_merges
  //      - social posts        → social_posts.lat/lon
  //      - video items         → video_items.lat/lon
  //      - uncertain cameras   → cameras.lat/lon (overwrites)
  cron.schedule('*/5 * * * *', () => {
    withCollectorRun('llmEnricher', () => runLlmEnricher(), { trigger: 'cron' })
      .catch((err) => {
        console.error('[scheduler] llmEnricher failed:', err?.message);
      });
  });
```

- [ ] **Step 3: Verify the server still boots**

Run: `cd server && timeout 6 node --env-file=../.env src/index.js || true`
Expected: server prints its startup banner; the process is killed by `timeout` after 6 seconds. No exceptions thrown during boot.

- [ ] **Step 4: Commit**

```bash
git add server/src/utils/scheduler.js
git commit -m "feat(server): register llmEnricher cron tick (every 5 minutes)"
```

---

## Task 9: Persist social_posts from socialMedia.js

**Files:**
- Modify: `server/src/collectors/socialMedia.js`

**Context:** Currently fetches Wikipedia GeoSearch and returns FeatureCollection in-memory. We persist hits into `social_posts` (always with `geo_source='native_geo'` because Wikipedia hits all carry coords) and read FeatureCollection back from the DB. This makes the collector compatible with future LLM-enriched social platforms.

- [ ] **Step 1: Replace the file**

```js
// server/src/collectors/socialMedia.js
import { fetchJson } from './_liveHelpers.js';
import db from '../utils/database.js';

const GEO_HUBS = [
  { area: 'Tokyo',    lat: 35.6812, lon: 139.7671 },
  { area: 'Osaka',    lat: 34.6937, lon: 135.5023 },
  { area: 'Kyoto',    lat: 35.0116, lon: 135.7681 },
  { area: 'Nagoya',   lat: 35.1815, lon: 136.9066 },
  { area: 'Fukuoka',  lat: 33.5902, lon: 130.4017 },
  { area: 'Sapporo',  lat: 43.0621, lon: 141.3544 },
  { area: 'Yokohama', lat: 35.4437, lon: 139.6380 },
  { area: 'Kobe',     lat: 34.6901, lon: 135.1955 },
  { area: 'Hiroshima',lat: 34.3853, lon: 132.4553 },
  { area: 'Sendai',   lat: 38.2682, lon: 140.8694 },
  { area: 'Naha',     lat: 26.2124, lon: 127.6809 },
];

const stmtUpsertPost = db.prepare(`
  INSERT INTO social_posts
    (post_uid, platform, author, text, title, url, media_urls, language,
     posted_at, lat, lon, geo_source, properties)
  VALUES
    (@post_uid, @platform, @author, @text, @title, @url, @media_urls, @language,
     @posted_at, @lat, @lon, @geo_source, @properties)
  ON CONFLICT(post_uid) DO UPDATE SET
    text = excluded.text,
    title = excluded.title,
    url = excluded.url,
    media_urls = excluded.media_urls,
    lat = COALESCE(social_posts.lat, excluded.lat),
    lon = COALESCE(social_posts.lon, excluded.lon),
    geo_source = COALESCE(social_posts.geo_source, excluded.geo_source)
`);

const stmtSelectGeocoded = db.prepare(`
  SELECT post_uid, platform, author, text, title, url, lat, lon, geo_source,
         llm_place_name, fetched_at
  FROM social_posts
  WHERE platform = 'wikipedia' AND lat IS NOT NULL AND lon IS NOT NULL
  ORDER BY fetched_at DESC
  LIMIT 5000
`);

async function fetchGeoArticles(hub) {
  const url =
    `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*` +
    `&list=geosearch&gscoord=${hub.lat}|${hub.lon}&gsradius=10000&gslimit=30`;
  const data = await fetchJson(url, { timeoutMs: 7000 });
  const pages = data?.query?.geosearch;
  if (!Array.isArray(pages)) return 0;
  let n = 0;
  for (const p of pages) {
    if (!Number.isFinite(p.lat) || !Number.isFinite(p.lon)) continue;
    stmtUpsertPost.run({
      post_uid: `WIKI_${p.pageid}`,
      platform: 'wikipedia',
      author: null,
      text: null,
      title: p.title,
      url: `https://en.wikipedia.org/?curid=${p.pageid}`,
      media_urls: null,
      language: 'en',
      posted_at: null,
      lat: p.lat,
      lon: p.lon,
      geo_source: 'native_geo',
      properties: JSON.stringify({ hub: hub.area }),
    });
    n++;
  }
  return n;
}

export default async function collectSocialMedia() {
  await Promise.all(GEO_HUBS.map((h) => fetchGeoArticles(h).catch(() => 0)));
  const rows = stmtSelectGeocoded.all();
  const features = rows.map((r) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      post_id: r.post_uid,
      platform: r.platform,
      content_type: 'article',
      area_name: r.title,
      url: r.url,
      timestamp: r.fetched_at,
      has_location: true,
      source: r.geo_source === 'llm_gsi' ? 'wikipedia_geosearch+llm' : 'wikipedia_geosearch',
    },
  }));
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'social_media',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'wikipedia_geosearch' : null,
      description: 'Geolocated Wikipedia articles around Japanese urban hubs (live only)',
    },
    metadata: {},
  };
}
```

- [ ] **Step 2: Smoke test the collector**

Run: `cd server && node -e "import('./src/collectors/socialMedia.js').then(async m => { const r = await m.default(); console.log('features:', r.features.length, 'live:', r._meta.live); })"` (timeout: 60s)
Expected: prints non-zero feature count if Wikipedia is reachable; 0 + `live:false` otherwise. No exceptions.

- [ ] **Step 3: Verify the DB has rows**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT count(*) as n FROM social_posts').get()))"`
Expected: prints `{ n: <some number ≥ 0> }`.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/socialMedia.js
git commit -m "feat(server): persist social_posts from socialMedia.js wikipedia hits"
```

---

## Task 10: Persist social_posts from twitterGeo.js (incl. ungeocoded Mastodon)

**Files:**
- Modify: `server/src/collectors/twitterGeo.js`

**Context:** Twitter API hits already have `geo.coordinates` → `geo_source='native_geo'`. Mastodon posts that contain a `JAPAN_PLACES` substring → `geo_source='place_match'` with that place's coords. **Mastodon posts that don't match any place name** are persisted with `lat=null` so the LLM enricher can pick them up. The returned FeatureCollection is sourced from the DB and only contains rows with `lat IS NOT NULL`.

- [ ] **Step 1: Replace the file**

```js
// server/src/collectors/twitterGeo.js
import { fetchJson } from './_liveHelpers.js';
import db from '../utils/database.js';

const BEARER_TOKEN = process.env.TWITTER_BEARER_TOKEN || '';

const MASTODON_INSTANCES = [
  'https://mstdn.jp',
  'https://pawoo.net',
  'https://mastodon-japan.net',
  'https://fedibird.com',
];

const JAPAN_PLACES = [
  { name: '渋谷', lat: 35.6595, lon: 139.7004 },
  { name: '新宿', lat: 35.6938, lon: 139.7036 },
  { name: '秋葉原', lat: 35.6984, lon: 139.7731 },
  { name: '原宿', lat: 35.6702, lon: 139.7035 },
  { name: '池袋', lat: 35.7295, lon: 139.7182 },
  { name: '六本木', lat: 35.6605, lon: 139.7292 },
  { name: '銀座', lat: 35.6717, lon: 139.7637 },
  { name: '浅草', lat: 35.7114, lon: 139.7966 },
  { name: 'お台場', lat: 35.6267, lon: 139.7752 },
  { name: '下北沢', lat: 35.6613, lon: 139.6680 },
  { name: '中目黒', lat: 35.6440, lon: 139.6988 },
  { name: '吉祥寺', lat: 35.7030, lon: 139.5795 },
  { name: '道頓堀', lat: 34.6687, lon: 135.5013 },
  { name: '心斎橋', lat: 34.6748, lon: 135.5012 },
  { name: '梅田', lat: 34.7055, lon: 135.4983 },
  { name: '難波', lat: 34.6627, lon: 135.5010 },
  { name: '天王寺', lat: 34.6468, lon: 135.5135 },
  { name: '河原町', lat: 35.0040, lon: 135.7693 },
  { name: '祇園', lat: 34.9986, lon: 135.7747 },
  { name: '嵐山', lat: 35.0170, lon: 135.6713 },
  { name: '博多', lat: 33.5920, lon: 130.4080 },
  { name: '天神', lat: 33.5898, lon: 130.3987 },
  { name: '栄', lat: 35.1692, lon: 136.9084 },
  { name: '横浜駅', lat: 35.4660, lon: 139.6223 },
  { name: '三宮', lat: 34.6951, lon: 135.1979 },
  { name: '札幌駅', lat: 43.0687, lon: 141.3508 },
  { name: 'すすきの', lat: 43.0556, lon: 141.3530 },
  { name: '国際通り', lat: 26.3358, lon: 127.6862 },
  { name: '仙台駅', lat: 38.2601, lon: 140.8822 },
  { name: '広島駅', lat: 34.3978, lon: 132.4752 },
];

const stmtUpsertPost = db.prepare(`
  INSERT INTO social_posts
    (post_uid, platform, author, text, title, url, media_urls, language,
     posted_at, lat, lon, geo_source, properties)
  VALUES
    (@post_uid, @platform, @author, @text, @title, @url, @media_urls, @language,
     @posted_at, @lat, @lon, @geo_source, @properties)
  ON CONFLICT(post_uid) DO UPDATE SET
    text = excluded.text,
    title = excluded.title,
    url = excluded.url,
    media_urls = excluded.media_urls,
    lat = COALESCE(social_posts.lat, excluded.lat),
    lon = COALESCE(social_posts.lon, excluded.lon),
    geo_source = COALESCE(social_posts.geo_source, excluded.geo_source)
`);

const stmtSelectGeocoded = db.prepare(`
  SELECT post_uid, platform, author, text, url, lat, lon, geo_source,
         llm_place_name, fetched_at, properties
  FROM social_posts
  WHERE platform IN ('twitter', 'mastodon')
    AND lat IS NOT NULL AND lon IS NOT NULL
  ORDER BY fetched_at DESC
  LIMIT 5000
`);

async function tryTwitterAPI() {
  if (!BEARER_TOKEN) return false;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const url = 'https://api.twitter.com/2/tweets/search/recent?query=place_country:JP has:geo&tweet.fields=geo,created_at,public_metrics&max_results=100';
    const res = await fetch(url, {
      headers: { Authorization: `Bearer ${BEARER_TOKEN}` },
      signal: controller.signal,
    });
    clearTimeout(timeout);
    if (!res.ok) return false;
    const data = await res.json();
    if (!Array.isArray(data?.data)) return false;
    for (const tweet of data.data) {
      const coords = tweet.geo?.coordinates?.coordinates;
      const username = tweet.author?.username || tweet.username || null;
      const tweetUrl = username
        ? `https://twitter.com/${username}/status/${tweet.id}`
        : `https://twitter.com/i/web/status/${tweet.id}`;
      const hasGeo = Array.isArray(coords) && coords.length === 2;
      stmtUpsertPost.run({
        post_uid: `TW_${tweet.id}`,
        platform: 'twitter',
        author: username,
        text: tweet.text,
        title: null,
        url: tweetUrl,
        media_urls: null,
        language: tweet.lang || null,
        posted_at: tweet.created_at || null,
        lat: hasGeo ? coords[1] : null,
        lon: hasGeo ? coords[0] : null,
        geo_source: hasGeo ? 'native_geo' : null,
        properties: JSON.stringify({
          likes: tweet.public_metrics?.like_count || 0,
          retweets: tweet.public_metrics?.retweet_count || 0,
        }),
      });
    }
    return true;
  } catch {
    clearTimeout(timeout);
    return false;
  }
}

async function tryMastodonPublic() {
  let any = false;
  for (const instance of MASTODON_INSTANCES) {
    const posts = await fetchJson(
      `${instance}/api/v1/timelines/public?local=true&limit=40`,
      { timeoutMs: 7000 },
    );
    if (!Array.isArray(posts)) continue;
    for (const p of posts) {
      const body = (p.content || '').replace(/<[^>]*>/g, '');
      let place = null;
      for (const candidate of JAPAN_PLACES) {
        if (body.includes(candidate.name)) { place = candidate; break; }
      }
      const mediaUrls = (p.media_attachments || [])
        .filter((m) => m.type === 'image' && m.url)
        .map((m) => m.url);
      stmtUpsertPost.run({
        post_uid: `MAST_${instance.replace('https://', '')}_${p.id}`,
        platform: 'mastodon',
        author: p.account?.acct || p.account?.username || null,
        text: body.slice(0, 1000),
        title: null,
        url: p.url || null,
        media_urls: mediaUrls.length ? JSON.stringify(mediaUrls) : null,
        language: p.language || 'ja',
        posted_at: p.created_at || null,
        lat: place ? place.lat : null,
        lon: place ? place.lon : null,
        geo_source: place ? 'place_match' : null,
        properties: JSON.stringify({
          instance: instance.replace('https://', ''),
          area: place?.name || null,
          favourites: p.favourites_count || 0,
        }),
      });
      any = true;
    }
  }
  return any;
}

export default async function collectTwitterGeo() {
  const twitterRan = await tryTwitterAPI();
  const mastoRan = await tryMastodonPublic();
  const liveSource = twitterRan ? 'twitter_api' : (mastoRan ? 'mastodon_public_api' : null);

  const rows = stmtSelectGeocoded.all();
  const features = rows.map((r) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      id: r.post_uid,
      platform: r.platform,
      username: r.author,
      text: r.text?.slice(0, 280) || null,
      url: r.url,
      timestamp: r.fetched_at,
      area: r.llm_place_name,
      source: r.geo_source === 'llm_gsi' ? `${r.platform}+llm` : `${r.platform}_${r.geo_source}`,
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'twitter_geo',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? liveSource : null,
      description: 'Geotagged social posts from Japan — Twitter/X API + Mastodon public timelines (live only)',
    },
    metadata: {},
  };
}
```

- [ ] **Step 2: Smoke test**

Run: `cd server && node -e "import('./src/collectors/twitterGeo.js').then(async m => { const r = await m.default(); console.log('features:', r.features.length); })"` (timeout: 60s)
Expected: prints a feature count (could be 0 if Twitter has no token and Mastodon returns nothing matching `JAPAN_PLACES`); no exceptions.

- [ ] **Step 3: Verify ungeocoded Mastodon posts landed**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT platform, count(*) as n, sum(case when lat is null then 1 else 0 end) as ungeocoded FROM social_posts GROUP BY platform').all()))"`
Expected: a row for `mastodon` (and possibly `twitter`); `ungeocoded` should be > 0 unless every fetched Mastodon post happened to mention a `JAPAN_PLACES` name.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/twitterGeo.js
git commit -m "feat(server): twitterGeo persists ungeocoded mastodon posts for LLM enrichment"
```

---

## Task 11: cameraDiscovery — flag uncertain rows

**Files:**
- Modify: `server/src/collectors/cameraDiscovery.js`

**Context:** The LLM enricher only acts on cameras whose `properties.location_uncertain = 1`. Existing logic in `cameraDiscovery.js` already produces lat/lon from various channels; some channels are more trustworthy than others. We need to mark uncertain channels' output. The exact set of "uncertain" channels lives in this file — read it, identify any path where the lat/lon comes from name-matching a hardcoded city list (rather than a real upstream coordinate), and add `location_uncertain: 1` to that path's `properties` object.

If no such path exists today, this task degrades to a no-op — the column infrastructure is in place; future "uncertain" channels can flip the flag.

- [ ] **Step 1: Inspect the discovery channel handling**

Run: `grep -n "discovery_channels\|location_\|uncertain\|fallback\|cityCentroid\|name.*[Mm]atch" server/src/collectors/cameraDiscovery.js | head -40`
Expected: a list of channel-specific paths. If the grep is empty, leave the file untouched and skip to Step 3.

- [ ] **Step 2: Add the flag**

Augment the `properties` object built by any channel whose lat/lon came from string-matching (rather than receiving a real coordinate from the upstream). Concrete pattern:

```js
properties: JSON.stringify({
  ...existingProps,
  location_uncertain: 1, // <-- add only on the uncertain channel paths
}),
```

Apply minimally and surgically — don't restructure the file. If it's not obvious which path qualifies, ask before guessing.

- [ ] **Step 3: Smoke check imports**

Run: `cd server && node -e "import('./src/collectors/cameraDiscovery.js').then(m => console.log('imports OK', typeof m.default))"`
Expected: prints `imports OK function`. No errors.

- [ ] **Step 4: Commit (only if a change was made)**

```bash
git diff --stat server/src/collectors/cameraDiscovery.js
# If the diff is empty: skip the commit. Otherwise:
git add server/src/collectors/cameraDiscovery.js
git commit -m "feat(server): cameraDiscovery flags location_uncertain rows for LLM"
```

---

## Task 12: Document the enricher

**Files:**
- Modify: `docs/collectors.md`

- [ ] **Step 1: Append the section**

Append to `docs/collectors.md`:

```markdown
## LLM Enricher (`llmEnricher`)

Async write-behind worker that uses a local LM Studio (OpenAI-compatible
HTTP at `http://localhost:1234`) to:

- Resolve uncertain station-merge pairs the string-fingerprint clusterer
  can't decide. Pair-level decisions are written to `llm_station_merges`;
  the next clusterer run honours rows with `same=1 AND confidence>=0.7`.
- Extract Japanese place names from social posts (`social_posts`) and
  video items (`video_items`) whose `lat` is null, then resolve the name
  through the existing GSI address-search API to get coordinates. Result
  is written back to the row's `lat / lon / llm_place_name / llm_geocoded_at`.
- Refine cameras whose `properties.location_uncertain = 1` — overwrites
  `cameras.lat / lon` with the LLM+GSI result, preserving the original in
  `properties.original_lat / original_lon`.

The worker runs every 5 minutes via cron. It short-circuits to a no-op
when `LLM_ENABLED != 'true'`. Vision-capable inference (sending images to
the model) is gated by `LLM_VISION=true`.

Configuration: `LLM_ENABLED`, `LLM_BASE_URL`, `LLM_MODEL`, `LLM_VISION`,
`LLM_BATCH_SIZE`, `LLM_TIMEOUT_MS`.

To re-process a row: clear its `llm_geocoded_at` (and `llm_failure`) column.
```

- [ ] **Step 2: Commit**

```bash
git add docs/collectors.md
git commit -m "docs: describe llmEnricher worker"
```

---

## Task 13: End-to-end smoke against a real LM Studio (manual)

**Files:**
- None (manual verification)

**Context:** Final sign-off. Requires the operator to start LM Studio with a small model loaded (e.g. `Llama-3.2-3B-Instruct` or `Qwen2.5-3B-Instruct`).

- [ ] **Step 1: Confirm LM Studio is reachable**

Run: `curl -s http://localhost:1234/v1/models`
Expected: a JSON envelope listing the loaded model's id.

- [ ] **Step 2: Run the worker once**

Run: `cd server && LLM_ENABLED=true LLM_BATCH_SIZE=5 node -e "import('./src/utils/llmEnricher.js').then(async m => { console.log(JSON.stringify(await m.runLlmEnricher(), null, 2)); })"`
Expected: prints `{ skipped: false, stationDedup: { decided: N }, social: { geocoded: N }, video: { geocoded: 0 }, cameras: { geocoded: N } }`. At least one of the counts should be positive depending on what's pending in the DB.

- [ ] **Step 3: Inspect a geocoded row**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT post_uid, lat, lon, geo_source, llm_place_name, llm_failure FROM social_posts WHERE llm_geocoded_at IS NOT NULL ORDER BY llm_geocoded_at DESC LIMIT 5').all()))"`
Expected: rows with either `geo_source='llm_gsi'` and finite lat/lon, or one of the failure sentinels in `llm_failure`.

- [ ] **Step 4: Re-run — should be a near-no-op**

Run: `cd server && LLM_ENABLED=true node -e "import('./src/utils/llmEnricher.js').then(async m => console.log(await m.runLlmEnricher()))"`
Expected: very low geocoded counts (only newly-fetched rows since the previous run).

---

## Self-review

**Spec coverage:**

- LM Studio hardcoded shape, base URL env-overridable → Task 2. ✅
- Async write-behind worker → Tasks 7, 8. ✅
- Null-column-as-queue, no cache, no job table → Tasks 3, 7. ✅
- Station dedup pair generator → Task 5. ✅
- Clusterer Pass 3 → Task 6. ✅
- Social geocoding (text + vision optional) → Tasks 4, 7, 10. ✅
- Video geocoding plumbing → Tasks 3, 4, 7. ✅
- Camera refinement with `location_uncertain` → Tasks 3, 7, 11. ✅
- GSI helper extraction → Task 1. ✅
- Failure sentinels (`__no_match__`, `__gsi_miss__`, `__bad_json__`) → Task 7. ✅
- Confidence gates (0.5 in enricher, 0.7 in clusterer Pass 3) → Tasks 6, 7. ✅
- All env vars referenced in code → Task 7. ✅
- Docs → Task 12. ✅
- Manual end-to-end → Task 13. ✅

**Removed (per user direction):** all `*.test.js` files, all `node --test` runs, all stub HTTP servers, all DB-cleanup `reset()` helpers. Smoke verification only via `node -e`.

**Placeholder scan:** No "TBD"/"TODO"/"add appropriate error handling" in code blocks. Task 11 explicitly allows a no-op outcome but is otherwise concrete.

**Type consistency:** `chat({ messages, jsonSchema, timeoutMs, baseUrl, model })` — Task 2 (definition), Task 7 (caller). `gsiAddressSearch(query, opts)` — Task 1 (definition), Task 7 (caller). `findUncertainStationPairs()` — no-args throughout. SQL column names match between schema (Task 3) and queries (Tasks 7, 9, 10).

**Note:** `LLM_ENRICH_INTERVAL_MS` from the spec is intentionally NOT wired — node-cron uses cron expressions, not ms. Cron is pinned to `*/5 * * * *`. Documented as "every 5 minutes" in Task 12 docs.
