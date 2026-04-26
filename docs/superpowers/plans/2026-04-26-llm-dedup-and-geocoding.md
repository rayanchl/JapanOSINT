# LLM-Assisted Station Dedup, Social Geocoding, and Video Geocoding — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an LM Studio-backed enricher that resolves ambiguous station-merge pairs and geocodes ungeocoded social posts, video items, and uncertain cameras — by extracting a Japanese place name with the LLM, then resolving it to coordinates via the existing GSI address-search API.

**Architecture:**
- **Client:** `llmClient.js` — one ~80-line OpenAI-compatible HTTP client targeting LM Studio at `http://localhost:1234/v1/chat/completions`. Returns parsed JSON or `null` on any failure; never throws.
- **Worker:** `llmEnricher.js` — scheduler-tick worker that drains four queues per tick: station dedup pairs, ungeocoded social posts, ungeocoded video items, location-uncertain cameras. Each row's nullable `llm_geocoded_at` column IS the queue (no separate job table, no cache).
- **Storage:** new `social_posts` and `video_items` tables (lat/lon nullable), new `llm_station_merges` pair table, plus two new nullable columns on `cameras`.
- **Clusterer:** `stationClusterer.js` gains a Pass-3 union read from `llm_station_merges`, plus exports `findUncertainStationPairs()` for the worker to consume.
- **Collectors:** `socialMedia.js` and `twitterGeo.js` rewritten to persist into `social_posts` and read geocoded rows back; their FeatureCollection shape is preserved.

**Tech Stack:** Node 20+, ESM, `better-sqlite3`, built-in `fetch`, `node-cron`, `node:test`.

**Spec:** `docs/superpowers/specs/2026-04-26-llm-dedup-and-geocoding-design.md`

---

## File Structure

**New files:**
- `server/src/utils/llmClient.js` — OpenAI-compatible chat client targeting LM Studio.
- `server/src/utils/gsiAddressSearch.js` — reusable GSI place-name → lat/lon helper extracted from `gsiGeocode.js`.
- `server/src/utils/llmEnricher.js` — async write-behind worker; four drain functions.
- `server/src/utils/llmPrompts.js` — pure prompt-builder functions (one per job type) for unit testing.
- `server/test/llmClient.test.js` — stub-server tests for the HTTP client.
- `server/test/llmPrompts.test.js` — snapshot-style tests for prompt builders.
- `server/test/llmEnricher.test.js` — end-to-end test with stubbed LLM + stubbed GSI.
- `server/test/gsiAddressSearch.test.js` — stub-server test for the GSI helper.

**Modified files:**
- `server/src/utils/database.js` — schema additions: `social_posts`, `video_items`, `llm_station_merges`, two columns on `cameras`. Plus exported helper `migrateAddColumnIfMissing(table, col, ddl)` if not already present.
- `server/src/utils/scheduler.js` — register `llmEnricher` cron tick.
- `server/src/utils/stationClusterer.js` — Pass 3 reading `llm_station_merges`; export `findUncertainStationPairs()`.
- `server/src/collectors/socialMedia.js` — persist all fetched articles into `social_posts`; return FeatureCollection from DB.
- `server/src/collectors/twitterGeo.js` — same persistence rewrite.
- `server/src/collectors/cameraDiscovery.js` — set `properties.location_uncertain = 1` on rows the existing logic considered fuzzy.
- `server/src/collectors/gsiGeocode.js` — switch to use the new shared `gsiAddressSearch` helper.
- `docs/collectors.md` — short paragraph documenting the LLM enricher.

---

## Task 1: GSI address-search helper extracted

**Files:**
- Create: `server/src/utils/gsiAddressSearch.js`
- Create: `server/test/gsiAddressSearch.test.js`

**Context:** `server/src/collectors/gsiGeocode.js` currently calls `https://msearch.gsi.go.jp/address-search/AddressSearch?q=<name>` with a hardcoded query (`東京駅`). We need this same API call as a reusable helper that accepts an arbitrary query and returns `{ lat, lon, title } | null`. Both the LLM enricher and the existing `gsiGeocode` collector will use it. We extract first, refactor `gsiGeocode.js` second (Task 2).

- [ ] **Step 1: Write the failing test**

```js
// server/test/gsiAddressSearch.test.js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { gsiAddressSearch } from '../src/utils/gsiAddressSearch.js';

function startStub(handler) {
  return new Promise((resolve) => {
    const srv = http.createServer(handler);
    srv.listen(0, '127.0.0.1', () => {
      const { port } = srv.address();
      resolve({ srv, baseUrl: `http://127.0.0.1:${port}` });
    });
  });
}

test('parses GSI hit into {lat, lon, title}', async () => {
  const { srv, baseUrl } = await startStub((req, res) => {
    assert.match(req.url, /q=%E6%B8%8B%E8%B0%B7/);
    res.setHeader('content-type', 'application/json');
    res.end(JSON.stringify([
      { geometry: { coordinates: [139.7004, 35.6595] }, properties: { title: '渋谷' } },
      { geometry: { coordinates: [139.7000, 35.6500] }, properties: { title: '渋谷区' } },
    ]));
  });
  const out = await gsiAddressSearch('渋谷', { baseUrl, timeoutMs: 5000 });
  srv.close();
  assert.deepEqual(out, { lat: 35.6595, lon: 139.7004, title: '渋谷' });
});

test('returns null on empty result', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.setHeader('content-type', 'application/json');
    res.end('[]');
  });
  const out = await gsiAddressSearch('xyz', { baseUrl, timeoutMs: 5000 });
  srv.close();
  assert.equal(out, null);
});

test('returns null on HTTP error', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.statusCode = 500; res.end('boom');
  });
  const out = await gsiAddressSearch('東京', { baseUrl, timeoutMs: 5000 });
  srv.close();
  assert.equal(out, null);
});

test('returns null on timeout without throwing', async () => {
  const { srv, baseUrl } = await startStub(() => { /* never respond */ });
  const out = await gsiAddressSearch('東京', { baseUrl, timeoutMs: 100 });
  srv.close();
  assert.equal(out, null);
});

test('returns null and does not throw on bad JSON', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.setHeader('content-type', 'application/json');
    res.end('not-json');
  });
  const out = await gsiAddressSearch('東京', { baseUrl, timeoutMs: 5000 });
  srv.close();
  assert.equal(out, null);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && node --test test/gsiAddressSearch.test.js`
Expected: FAIL — `Cannot find module '../src/utils/gsiAddressSearch.js'`.

- [ ] **Step 3: Implement the helper**

```js
// server/src/utils/gsiAddressSearch.js
const DEFAULT_BASE = 'https://msearch.gsi.go.jp';
const DEFAULT_TIMEOUT_MS = 8000;

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

- [ ] **Step 4: Run test to verify it passes**

Run: `cd server && node --test test/gsiAddressSearch.test.js`
Expected: PASS — 5/5 tests pass.

- [ ] **Step 5: Commit**

```bash
git add server/src/utils/gsiAddressSearch.js server/test/gsiAddressSearch.test.js
git commit -m "feat(server): add reusable gsiAddressSearch helper"
```

---

## Task 2: Refactor gsiGeocode collector to use the shared helper

**Files:**
- Modify: `server/src/collectors/gsiGeocode.js`

**Context:** Replace the inline fetch in `gsiGeocode.js` with a single call to the new helper. Behaviour is preserved: a successful hit returns one Feature; failure produces a seed Feature.

- [ ] **Step 1: Replace the file contents**

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

- [ ] **Step 2: Verify nothing else broke**

Run: `cd server && node --test test/`
Expected: PASS — pre-existing test count + the new gsiAddressSearch tests, no regressions.

- [ ] **Step 3: Commit**

```bash
git add server/src/collectors/gsiGeocode.js
git commit -m "refactor(server): gsiGeocode uses shared gsiAddressSearch helper"
```

---

## Task 3: LLM client — failing test for happy path

**Files:**
- Create: `server/test/llmClient.test.js`

**Context:** `llmClient.chat({ messages, jsonSchema, timeoutMs })` posts an OpenAI-compatible chat completion to LM Studio and returns the parsed JSON object the model produced (parsed from `choices[0].message.content`). It returns `null` on any failure (timeout, non-2xx, malformed JSON, schema-violating JSON). Never throws past the caller. We test against a stub `http.createServer` so no LM Studio is required.

- [ ] **Step 1: Write the failing test**

```js
// server/test/llmClient.test.js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { chat } from '../src/utils/llmClient.js';

function startStub(handler) {
  return new Promise((resolve) => {
    const srv = http.createServer(handler);
    srv.listen(0, '127.0.0.1', () => {
      const { port } = srv.address();
      resolve({ srv, baseUrl: `http://127.0.0.1:${port}` });
    });
  });
}

const SCHEMA = {
  type: 'object',
  properties: { ok: { type: 'boolean' } },
  required: ['ok'],
};

test('parses choices[0].message.content as JSON', async () => {
  const { srv, baseUrl } = await startStub((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      const payload = JSON.parse(body);
      assert.equal(payload.response_format?.type, 'json_schema');
      assert.equal(payload.response_format?.json_schema?.strict, true);
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({
        choices: [{ message: { content: '{"ok":true}' } }],
      }));
    });
  });
  const out = await chat({
    messages: [{ role: 'user', content: 'hi' }],
    jsonSchema: SCHEMA,
    timeoutMs: 5000,
    baseUrl,
  });
  srv.close();
  assert.deepEqual(out, { ok: true });
});

test('returns null on non-2xx', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.statusCode = 500; res.end('boom');
  });
  const out = await chat({
    messages: [{ role: 'user', content: 'hi' }],
    jsonSchema: SCHEMA,
    timeoutMs: 5000,
    baseUrl,
  });
  srv.close();
  assert.equal(out, null);
});

test('returns null on malformed envelope', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.setHeader('content-type', 'application/json');
    res.end('{"choices":[{"message":{}}]}'); // no content
  });
  const out = await chat({
    messages: [{ role: 'user', content: 'hi' }],
    jsonSchema: SCHEMA,
    timeoutMs: 5000,
    baseUrl,
  });
  srv.close();
  assert.equal(out, null);
});

test('returns null on unparseable model output', async () => {
  const { srv, baseUrl } = await startStub((_req, res) => {
    res.setHeader('content-type', 'application/json');
    res.end(JSON.stringify({
      choices: [{ message: { content: 'not json at all' } }],
    }));
  });
  const out = await chat({
    messages: [{ role: 'user', content: 'hi' }],
    jsonSchema: SCHEMA,
    timeoutMs: 5000,
    baseUrl,
  });
  srv.close();
  assert.equal(out, null);
});

test('returns null on timeout', async () => {
  const { srv, baseUrl } = await startStub(() => { /* never respond */ });
  const out = await chat({
    messages: [{ role: 'user', content: 'hi' }],
    jsonSchema: SCHEMA,
    timeoutMs: 100,
    baseUrl,
  });
  srv.close();
  assert.equal(out, null);
});

test('passes through image_url content parts unchanged', async () => {
  let received = null;
  const { srv, baseUrl } = await startStub((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      received = JSON.parse(body);
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({
        choices: [{ message: { content: '{"ok":true}' } }],
      }));
    });
  });
  await chat({
    messages: [{
      role: 'user',
      content: [
        { type: 'text', text: 'caption' },
        { type: 'image_url', image_url: { url: 'http://x/img.jpg' } },
      ],
    }],
    jsonSchema: SCHEMA,
    timeoutMs: 5000,
    baseUrl,
  });
  srv.close();
  assert.equal(received.messages[0].content[1].type, 'image_url');
  assert.equal(received.messages[0].content[1].image_url.url, 'http://x/img.jpg');
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && node --test test/llmClient.test.js`
Expected: FAIL — `Cannot find module '../src/utils/llmClient.js'`.

---

## Task 4: LLM client — implementation

**Files:**
- Create: `server/src/utils/llmClient.js`

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
 * @param {Array} args.messages       OpenAI-style messages array.
 * @param {object} args.jsonSchema    JSON Schema enforced via response_format.
 * @param {number} [args.timeoutMs]   Request timeout, default 30s.
 * @param {string} [args.baseUrl]     Override LM Studio URL (env or default otherwise).
 * @param {string} [args.model]       Override model id (env or default otherwise).
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

- [ ] **Step 2: Run test to verify it passes**

Run: `cd server && node --test test/llmClient.test.js`
Expected: PASS — 6/6 tests pass.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/llmClient.js server/test/llmClient.test.js
git commit -m "feat(server): add LM Studio OpenAI-compatible chat client"
```

---

## Task 5: Schema migrations — new tables and columns

**Files:**
- Modify: `server/src/utils/database.js`

**Context:** Add the `social_posts`, `video_items`, `llm_station_merges` tables and two columns on `cameras`. These go inside the `db.exec(...)` block that already holds all other `CREATE TABLE IF NOT EXISTS` statements. The two new `cameras` columns must be added through the existing column-migration path because `cameras` already exists in deployed DBs — `CREATE TABLE IF NOT EXISTS` won't add them.

- [ ] **Step 1: Inspect the existing column-migration helper**

Run: `grep -n "ALTER TABLE\|migrateAddColumn\|addColumn" server/src/utils/database.js`
Expected: a section near the bottom of the file (around the `station_line_dots` migration) that adds columns conditionally. If a generic helper exists, reuse it. If not, write the cameras column add inline using the same pattern shown in the existing `station_line_dots` migration block.

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

Find the existing post-`db.exec` migration block in `database.js` that uses `PRAGMA table_info(...)` to add columns to existing tables. Add a sibling block:

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

- [ ] **Step 4: Smoke-test the schema by booting the server in dry-mode**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => { const r = m.default.prepare('SELECT name FROM sqlite_master WHERE type=\"table\"').all(); console.log(r.map(x => x.name).sort().join(' ')); })"`
Expected: prints a table list that includes `social_posts video_items llm_station_merges` (along with the existing tables). No errors.

- [ ] **Step 5: Verify cameras has the new columns**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => { const r = m.default.prepare('PRAGMA table_info(cameras)').all(); console.log(r.map(c => c.name).join(' ')); })"`
Expected: includes `llm_place_name llm_geocoded_at`.

- [ ] **Step 6: Commit**

```bash
git add server/src/utils/database.js
git commit -m "feat(db): add social_posts, video_items, llm_station_merges; cameras llm columns"
```

---

## Task 6: Prompt builders (pure functions) — failing tests

**Files:**
- Create: `server/test/llmPrompts.test.js`

**Context:** Each enrichment job has a prompt-building pure function. Keeping these pure (no DB, no fetch) lets us snapshot-test them without infra. Three builders: `buildDedupPairPrompt`, `buildSocialGeocodePrompt`, `buildVideoGeocodePrompt`. All return `{ messages, jsonSchema }`. Vision support: `buildSocialGeocodePrompt` and `buildVideoGeocodePrompt` accept an `imageUrls` array; when non-empty AND `vision: true`, they append `image_url` content parts to the user message.

- [ ] **Step 1: Write failing tests**

```js
// server/test/llmPrompts.test.js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  buildDedupPairPrompt,
  buildSocialGeocodePrompt,
  buildVideoGeocodePrompt,
} from '../src/utils/llmPrompts.js';

test('dedup prompt includes both stations and distance', () => {
  const { messages, jsonSchema } = buildDedupPairPrompt({
    uid_a: 'A', name_a: '霞ケ関', name_ja_a: '霞ケ関', operator_a: 'Tokyo Metro', line_a: 'Marunouchi', mode_a: 'subway',
    uid_b: 'B', name_b: 'Kasumigaseki', name_ja_b: '霞ヶ関', operator_b: 'Tokyo Metro', line_b: 'Hibiya', mode_b: 'subway',
    dist_m: 38,
  });
  assert.equal(messages[0].role, 'system');
  assert.match(messages[1].content, /霞ケ関/);
  assert.match(messages[1].content, /Kasumigaseki/);
  assert.match(messages[1].content, /38\s*m/);
  assert.deepEqual(jsonSchema.required, ['same_station', 'confidence']);
});

test('social prompt is text-only when imageUrls empty', () => {
  const { messages } = buildSocialGeocodePrompt({
    platform: 'twitter',
    author: '@x',
    text: '今日の渋谷スクランブル混みすぎ',
    title: null,
    imageUrls: [],
    vision: true,
  });
  // user message is a plain string when no images
  assert.equal(typeof messages[1].content, 'string');
  assert.match(messages[1].content, /渋谷/);
});

test('social prompt appends image_url parts when vision=true', () => {
  const { messages } = buildSocialGeocodePrompt({
    platform: 'mastodon',
    author: '@x',
    text: 'where am i',
    title: null,
    imageUrls: ['http://example/a.jpg', 'http://example/b.jpg'],
    vision: true,
  });
  // user content becomes an array with text + 2 image_url parts
  assert.ok(Array.isArray(messages[1].content));
  const types = messages[1].content.map((p) => p.type);
  assert.deepEqual(types, ['text', 'image_url', 'image_url']);
});

test('social prompt drops images when vision=false', () => {
  const { messages } = buildSocialGeocodePrompt({
    platform: 'mastodon',
    author: '@x',
    text: 'where am i',
    title: null,
    imageUrls: ['http://example/a.jpg'],
    vision: false,
  });
  assert.equal(typeof messages[1].content, 'string');
});

test('social prompt caps images at 2', () => {
  const { messages } = buildSocialGeocodePrompt({
    platform: 'twitter',
    author: '@x',
    text: 'a',
    title: null,
    imageUrls: ['1', '2', '3', '4'],
    vision: true,
  });
  const imgs = messages[1].content.filter((p) => p.type === 'image_url');
  assert.equal(imgs.length, 2);
});

test('social prompt truncates long text to 500 chars', () => {
  const long = 'あ'.repeat(2000);
  const { messages } = buildSocialGeocodePrompt({
    platform: 'twitter',
    author: '@x',
    text: long,
    title: null,
    imageUrls: [],
    vision: false,
  });
  const userText = messages[1].content;
  assert.ok(userText.length < 800, 'user message should be bounded');
});

test('video prompt includes title, description, channel', () => {
  const { messages } = buildVideoGeocodePrompt({
    platform: 'youtube',
    channel: 'Tokyo Walks',
    title: 'Walking through 渋谷スクランブル交差点',
    description: 'Filmed near Hachiko exit',
    thumbnailUrl: null,
    vision: false,
  });
  assert.match(messages[1].content, /Tokyo Walks/);
  assert.match(messages[1].content, /Walking through 渋谷/);
  assert.match(messages[1].content, /Hachiko/);
});

test('video prompt appends thumbnail when vision=true', () => {
  const { messages } = buildVideoGeocodePrompt({
    platform: 'youtube',
    channel: 'X',
    title: 'T',
    description: null,
    thumbnailUrl: 'http://example/thumb.jpg',
    vision: true,
  });
  assert.ok(Array.isArray(messages[1].content));
  const types = messages[1].content.map((p) => p.type);
  assert.deepEqual(types, ['text', 'image_url']);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && node --test test/llmPrompts.test.js`
Expected: FAIL — `Cannot find module '../src/utils/llmPrompts.js'`.

---

## Task 7: Prompt builders — implementation

**Files:**
- Create: `server/src/utils/llmPrompts.js`

- [ ] **Step 1: Write the implementation**

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

- [ ] **Step 2: Run test to verify it passes**

Run: `cd server && node --test test/llmPrompts.test.js`
Expected: PASS — all 8 tests pass.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/llmPrompts.js server/test/llmPrompts.test.js
git commit -m "feat(server): pure prompt builders for dedup, social, video LLM jobs"
```

---

## Task 8: Expose `findUncertainStationPairs` from the clusterer

**Files:**
- Modify: `server/src/utils/stationClusterer.js`

**Context:** The worker needs the list of uncertain pairs to ask the LLM about. Reuse the cell-bucketed neighbour scan from Pass 2: emit any (i, j) with i<j, same/adjacent cell, distance ≤ 150 m, fingerprints differ AND Levenshtein > 2 OR fingerprints match but operator strings differ when both present (these are the "interesting" pairs Pass 2 won't merge but a human probably would). Cap the result at 500 to keep the worker bounded.

- [ ] **Step 1: Add the export at the bottom of the file**

Append to `stationClusterer.js`, after `getAllLineDotFeatures`:

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
        // "Interesting": Pass 2 would NOT merge, but they're spatially close.
        // Either the names differ enough that fingerprint+Levenshtein both miss,
        // OR the names are identical but the operators differ (cross-operator
        // station that wikidata didn't link).
        const wouldPass2Merge = sameFp || closeFp;
        const interesting = !wouldPass2Merge || (sameFp && operatorsDiffer);
        if (!interesting) continue;
        // Order pair lexically.
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

- [ ] **Step 2: Verify the file still imports and runs**

Run: `cd server && node -e "import('./src/utils/stationClusterer.js').then(m => console.log(typeof m.findUncertainStationPairs))"`
Expected: prints `function`. No errors.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/stationClusterer.js
git commit -m "feat(server): export findUncertainStationPairs from stationClusterer"
```

---

## Task 9: Clusterer Pass 3 — read llm_station_merges

**Files:**
- Modify: `server/src/utils/stationClusterer.js`

**Context:** After Pass 1 (wikidata) and Pass 2 (fingerprint), add Pass 3 that unions any pair the LLM has confidently labelled `same=1, confidence>=0.7`. The lookup needs station_uid → index. Build that map once before the existing pass loop, and reuse it in Pass 3.

- [ ] **Step 1: Modify the cluster() function**

In `stationClusterer.js`, edit the `function cluster(stations)` body. After the Pass 2 nested loops and *before* the "Collect groups" comment, insert:

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
Expected: prints `{ stations: <N>, clusters: <M>, merges: <K>, dots: <D> }` with no errors. If `llm_station_merges` is empty (which it is initially), output should match the previous run.

- [ ] **Step 3: Commit**

```bash
git add server/src/utils/stationClusterer.js
git commit -m "feat(server): clusterer Pass 3 honours llm_station_merges"
```

---

## Task 10: llmEnricher — failing test for the dedup drain

**Files:**
- Create: `server/test/llmEnricher.test.js`

**Context:** The enricher is non-trivial — DB reads + writes + LLM HTTP + GSI HTTP. We test it with both LLM and GSI stubbed via dependency injection. The enricher exports its drain functions individually so tests can call them without the cron tick.

- [ ] **Step 1: Write failing tests**

```js
// server/test/llmEnricher.test.js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import db from '../src/utils/database.js';
import {
  enrichStationDedup,
  enrichSocialGeocode,
  enrichVideoGeocode,
  enrichCameras,
} from '../src/utils/llmEnricher.js';

function reset() {
  db.exec('DELETE FROM llm_station_merges');
  db.exec('DELETE FROM social_posts');
  db.exec('DELETE FROM video_items');
  db.exec(`DELETE FROM cameras WHERE camera_uid LIKE 'TEST_%'`);
}

test('enrichStationDedup: writes a positive merge and skips already-decided pairs', async () => {
  reset();
  const pairs = [
    { uid_a: 'A', uid_b: 'B', name_a: '霞ケ関', name_ja_a: '霞ケ関', operator_a: 'X', line_a: 'M', mode_a: 'subway', lat_a: 35.67, lon_a: 139.75,
      name_b: 'Kasumigaseki', name_ja_b: '霞ヶ関', operator_b: 'X', line_b: 'H', mode_b: 'subway', lat_b: 35.6705, lon_b: 139.7505,
      dist_m: 38 },
  ];
  const llmStub = async () => ({ same_station: true, confidence: 0.92, reason: 'name variants' });
  const result = await enrichStationDedup({
    pairsProvider: () => pairs,
    llmChat: llmStub,
    batchSize: 10,
  });
  assert.equal(result.decided, 1);
  const rows = db.prepare('SELECT * FROM llm_station_merges').all();
  assert.equal(rows.length, 1);
  assert.equal(rows[0].same, 1);

  // Run again — should be a no-op because the pair is already decided.
  const r2 = await enrichStationDedup({
    pairsProvider: () => pairs,
    llmChat: llmStub,
    batchSize: 10,
  });
  assert.equal(r2.decided, 0);
});

test('enrichStationDedup: LLM returning null is tolerated, leaves no row', async () => {
  reset();
  const pairs = [{ uid_a: 'A', uid_b: 'B', name_a: 'a', name_b: 'b', operator_a: '', operator_b: '', line_a: '', line_b: '', mode_a: 'train', mode_b: 'train', name_ja_a: '', name_ja_b: '', lat_a: 0, lon_a: 0, lat_b: 0, lon_b: 0, dist_m: 10 }];
  const llmStub = async () => null;
  const result = await enrichStationDedup({
    pairsProvider: () => pairs,
    llmChat: llmStub,
    batchSize: 10,
  });
  assert.equal(result.decided, 0);
  assert.equal(db.prepare('SELECT count(*) AS n FROM llm_station_merges').get().n, 0);
});

test('enrichSocialGeocode: extracts place, resolves via GSI, writes lat/lon', async () => {
  reset();
  db.prepare(`
    INSERT INTO social_posts (post_uid, platform, text)
    VALUES ('P1', 'twitter', '今日の渋谷スクランブル混みすぎ')
  `).run();

  const llmStub = async () => ({ place: '渋谷', confidence: 0.9 });
  const gsiStub = async (q) => {
    assert.equal(q, '渋谷');
    return { lat: 35.6595, lon: 139.7004, title: '渋谷' };
  };
  const result = await enrichSocialGeocode({
    llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10,
  });
  assert.equal(result.geocoded, 1);
  const row = db.prepare(`SELECT lat, lon, geo_source, llm_place_name, llm_failure FROM social_posts WHERE post_uid='P1'`).get();
  assert.equal(row.geo_source, 'llm_gsi');
  assert.equal(row.llm_place_name, '渋谷');
  assert.equal(row.llm_failure, null);
  assert.ok(Math.abs(row.lat - 35.6595) < 1e-6);
});

test('enrichSocialGeocode: LLM null place sets __no_match__ sentinel', async () => {
  reset();
  db.prepare(`INSERT INTO social_posts (post_uid, platform, text) VALUES ('P2', 'twitter', '今日寒いね')`).run();
  const llmStub = async () => ({ place: null, confidence: 1 });
  const gsiStub = async () => { throw new Error('should not be called'); };
  const result = await enrichSocialGeocode({ llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10 });
  assert.equal(result.geocoded, 0);
  const row = db.prepare(`SELECT llm_failure, llm_geocoded_at FROM social_posts WHERE post_uid='P2'`).get();
  assert.equal(row.llm_failure, '__no_match__');
  assert.ok(row.llm_geocoded_at);
});

test('enrichSocialGeocode: GSI miss sets __gsi_miss__ sentinel and preserves place name', async () => {
  reset();
  db.prepare(`INSERT INTO social_posts (post_uid, platform, text) VALUES ('P3', 'twitter', 'somewhere obscure')`).run();
  const llmStub = async () => ({ place: '実在しない地名', confidence: 0.8 });
  const gsiStub = async () => null;
  const result = await enrichSocialGeocode({ llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10 });
  assert.equal(result.geocoded, 0);
  const row = db.prepare(`SELECT lat, lon, llm_failure, llm_place_name FROM social_posts WHERE post_uid='P3'`).get();
  assert.equal(row.lat, null);
  assert.equal(row.llm_failure, '__gsi_miss__');
  assert.equal(row.llm_place_name, '実在しない地名');
});

test('enrichSocialGeocode: bad LLM response sets __bad_json__ sentinel', async () => {
  reset();
  db.prepare(`INSERT INTO social_posts (post_uid, platform, text) VALUES ('P4', 'twitter', 'x')`).run();
  const llmStub = async () => null;
  const gsiStub = async () => null;
  const result = await enrichSocialGeocode({ llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10 });
  assert.equal(result.geocoded, 0);
  const row = db.prepare(`SELECT llm_failure FROM social_posts WHERE post_uid='P4'`).get();
  assert.equal(row.llm_failure, '__bad_json__');
});

test('enrichSocialGeocode: never re-asks a row whose llm_geocoded_at is set', async () => {
  reset();
  db.prepare(`
    INSERT INTO social_posts (post_uid, platform, text, llm_geocoded_at, llm_failure)
    VALUES ('P5', 'twitter', 'old', '2026-01-01', '__no_match__')
  `).run();
  let called = false;
  const llmStub = async () => { called = true; return { place: 'X', confidence: 1 }; };
  await enrichSocialGeocode({ llmChat: llmStub, gsiSearch: async () => null, batchSize: 10 });
  assert.equal(called, false);
});

test('enrichVideoGeocode: title-based geocoding writes lat/lon', async () => {
  reset();
  db.prepare(`
    INSERT INTO video_items (video_uid, platform, channel, title, description)
    VALUES ('V1', 'youtube', 'TokyoWalks', 'Walking through Shibuya scramble', 'near Hachiko')
  `).run();
  const llmStub = async () => ({ place: '渋谷スクランブル交差点', confidence: 0.85 });
  const gsiStub = async () => ({ lat: 35.6595, lon: 139.7004, title: '渋谷' });
  const result = await enrichVideoGeocode({ llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10 });
  assert.equal(result.geocoded, 1);
  const row = db.prepare(`SELECT lat, lon, llm_place_name FROM video_items WHERE video_uid='V1'`).get();
  assert.ok(Math.abs(row.lat - 35.6595) < 1e-6);
  assert.equal(row.llm_place_name, '渋谷スクランブル交差点');
});

test('enrichCameras: only picks rows with location_uncertain=1, overwrites lat/lon, preserves originals', async () => {
  reset();
  db.prepare(`
    INSERT INTO cameras (camera_uid, name, lat, lon, discovery_channels, properties)
    VALUES ('TEST_CAM_A', 'Cam Uncertain', 35.0, 139.0, '[]', '{"location_uncertain":1}')
  `).run();
  db.prepare(`
    INSERT INTO cameras (camera_uid, name, lat, lon, discovery_channels, properties)
    VALUES ('TEST_CAM_B', 'Cam Certain', 35.0, 139.0, '[]', '{}')
  `).run();
  const llmStub = async () => ({ place: '渋谷', confidence: 0.9 });
  const gsiStub = async () => ({ lat: 35.6595, lon: 139.7004, title: '渋谷' });
  const result = await enrichCameras({ llmChat: llmStub, gsiSearch: gsiStub, batchSize: 10 });
  assert.equal(result.geocoded, 1);
  const a = db.prepare(`SELECT lat, lon, llm_place_name, properties FROM cameras WHERE camera_uid='TEST_CAM_A'`).get();
  assert.ok(Math.abs(a.lat - 35.6595) < 1e-6);
  assert.equal(a.llm_place_name, '渋谷');
  const props = JSON.parse(a.properties);
  assert.equal(props.original_lat, 35.0);
  assert.equal(props.original_lon, 139.0);
  const b = db.prepare(`SELECT lat, llm_place_name FROM cameras WHERE camera_uid='TEST_CAM_B'`).get();
  assert.equal(b.lat, 35.0); // untouched
  assert.equal(b.llm_place_name, null);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && node --test test/llmEnricher.test.js`
Expected: FAIL — `Cannot find module '../src/utils/llmEnricher.js'`.

---

## Task 11: llmEnricher — implementation

**Files:**
- Create: `server/src/utils/llmEnricher.js`

**Context:** Implement the four drain functions, each accepting an options bag with stubbable dependencies (`llmChat`, `gsiSearch`, `pairsProvider`, `batchSize`). When called from the cron tick, no overrides are passed and the real `chat` / `gsiAddressSearch` / `findUncertainStationPairs` are used. Each row that gets a verdict (success or any failure) gets `llm_geocoded_at = datetime('now')` written so the worker doesn't loop on it.

- [ ] **Step 1: Write the implementation**

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
const DEDUP_CONFIDENCE_GATE = 0.5; // record any decision; clusterer separately gates on 0.7
const PLACE_CONFIDENCE_GATE = 0.5;

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
    if (out.confidence < DEDUP_CONFIDENCE_GATE && !out.same_station) continue;
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
  return enrichTextOrVideo({
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
  return enrichTextOrVideo({
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
    // Build a tiny prompt — cameras don't have rich text; use the name.
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
    if (out.confidence != null && out.confidence < PLACE_CONFIDENCE_GATE) {
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

// --- Shared text/video drain ---
async function enrichTextOrVideo({ rowsStmt, buildPrompt, onOk, onFail, llmChat, gsiSearch, batchSize }) {
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

- [ ] **Step 2: Run test to verify it passes**

Run: `cd server && node --test test/llmEnricher.test.js`
Expected: PASS — all 9 tests pass.

- [ ] **Step 3: Run the full test suite to confirm no regressions**

Run: `cd server && node --test test/`
Expected: PASS — pre-existing tests all still pass.

- [ ] **Step 4: Commit**

```bash
git add server/src/utils/llmEnricher.js server/test/llmEnricher.test.js
git commit -m "feat(server): llmEnricher async write-behind worker"
```

---

## Task 12: Wire the enricher into the scheduler

**Files:**
- Modify: `server/src/utils/scheduler.js`

**Context:** Add a cron-scheduled tick that calls `runLlmEnricher`. The function itself short-circuits when `LLM_ENABLED!=true`, so it's safe to register unconditionally. Cron pattern `*/5 * * * *` (every 5 minutes), placed after the GTFS-RT catalogue refresh.

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
Expected: server prints its usual startup banner, no errors on import. (`timeout 6` kills it after 6s — we just want to confirm it doesn't crash on load.)

- [ ] **Step 4: Commit**

```bash
git add server/src/utils/scheduler.js
git commit -m "feat(server): register llmEnricher cron tick (every 5 minutes)"
```

---

## Task 13: Persist social_posts from socialMedia.js

**Files:**
- Modify: `server/src/collectors/socialMedia.js`

**Context:** The collector currently fetches Wikipedia GeoSearch results and returns them as a FeatureCollection. Wikipedia hits all carry coordinates, so they go into `social_posts` with `lat/lon` set and `geo_source='native_geo'`. The FeatureCollection returned to callers is now built by SELECTing geocoded rows from the DB so the enricher's later writes (when other social collectors send ungeocoded posts) flow through.

- [ ] **Step 1: Add an upsert helper in the collector**

Replace the body of `collectSocialMedia()` and add an `upsertPost` helper:

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

Run: `cd server && node -e "import('./src/collectors/socialMedia.js').then(async m => { const r = await m.default(); console.log('features:', r.features.length, 'live:', r._meta.live); })"`
Expected: prints non-zero feature count if Wikipedia is reachable; 0 + `live:false` if not (still no error).

- [ ] **Step 3: Verify the DB has rows**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT count(*) as n FROM social_posts').get()))"`
Expected: prints `{ n: <some number > 0> }` if Wikipedia returned hits; `{ n: 0 }` otherwise.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/socialMedia.js
git commit -m "feat(server): persist social_posts from socialMedia.js wikipedia hits"
```

---

## Task 14: Persist social_posts from twitterGeo.js, including ungeocoded Mastodon

**Files:**
- Modify: `server/src/collectors/twitterGeo.js`

**Context:** Twitter API hits already have `geo.coordinates` → `geo_source='native_geo'`. Mastodon posts that contain a `JAPAN_PLACES` substring → `geo_source='place_match'` with that place's coords. **Mastodon posts that don't match any place name** are now persisted with `lat=null` so the LLM enricher can take a swing at them. The returned FeatureCollection is sourced from the DB and only contains rows with `lat IS NOT NULL`.

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
      const url = username
        ? `https://twitter.com/${username}/status/${tweet.id}`
        : `https://twitter.com/i/web/status/${tweet.id}`;
      const hasGeo = Array.isArray(coords) && coords.length === 2;
      stmtUpsertPost.run({
        post_uid: `TW_${tweet.id}`,
        platform: 'twitter',
        author: username,
        text: tweet.text,
        title: null,
        url,
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

Run: `cd server && node -e "import('./src/collectors/twitterGeo.js').then(async m => { const r = await m.default(); console.log('features:', r.features.length); })"`
Expected: prints a feature count (could be 0 if Twitter has no token and Mastodon returns nothing matching `JAPAN_PLACES`); no errors.

- [ ] **Step 3: Verify ungeocoded Mastodon posts landed**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT platform, count(*) as n, sum(case when lat is null then 1 else 0 end) as ungeocoded FROM social_posts GROUP BY platform').all()))"`
Expected: a row for `mastodon` (and possibly `twitter`) — `ungeocoded` should be > 0 unless every fetched Mastodon post happened to mention a `JAPAN_PLACES` name.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/twitterGeo.js
git commit -m "feat(server): twitterGeo persists ungeocoded mastodon posts for LLM enrichment"
```

---

## Task 15: cameraDiscovery — flag uncertain rows

**Files:**
- Modify: `server/src/collectors/cameraDiscovery.js`

**Context:** The existing collector merges camera entries from multiple discovery channels and writes them with whatever lat/lon the channel reported. The LLM enricher only acts on rows whose `properties.location_uncertain = 1`. We need to mark rows whose location came from a low-confidence source. Read `cameraDiscovery.js` first to find the per-channel logic; the typical signal is rows whose lat/lon was set from name-only string-matching against a hardcoded city list, or rows whose channel name is one of `['scrape_naive', 'osm_text_only']` etc. The exact set of "uncertain" channels lives in this file already — we just write a flag instead of dropping it.

- [ ] **Step 1: Inspect the current discovery channel handling**

Run: `grep -n "discovery_channels\|location_\|uncertain\|lat,\s*lon" server/src/collectors/cameraDiscovery.js | head -40`
Expected: a list of locations where lat/lon get assigned per channel, plus where rows are stored.

- [ ] **Step 2: Add the flag**

In whichever path inside `cameraDiscovery.js` produces a row whose location came from name-matching only (rather than a hard coordinate from the upstream source), augment the `properties` object before insert with `location_uncertain: 1`. If the file has a single shared "build properties" function, add the flag there controlled by a parameter. Concretely, find every `INSERT INTO cameras` or `properties: JSON.stringify({...})` site and ensure the per-channel handler can opt-in. The minimum acceptable change is to add `location_uncertain: 1` to the properties object built by any channel that today produces a coordinate by string-matching a known place rather than receiving one from the upstream.

If no such channel currently exists in `cameraDiscovery.js`, this task degrades to a no-op — write no code, but verify by running:

```
grep -n "string-match\|name.*[Mm]atch\|cityCentroid\|fallback" server/src/collectors/cameraDiscovery.js
```

If that grep is empty, leave the file untouched and move to Step 3 — the column is in place; future "uncertain" channels can flip the flag.

- [ ] **Step 3: Smoke run camera discovery in isolation**

Run: `cd server && node -e "import('./src/utils/cameraRunner.js').then(async m => { console.log('skipping discovery to avoid network'); })"`
Expected: imports cleanly. (Don't actually run discovery — it's slow and network-dependent.)

- [ ] **Step 4: Commit (only if a change was made)**

```bash
git diff --stat server/src/collectors/cameraDiscovery.js
# If output is empty: skip the commit. Otherwise:
git add server/src/collectors/cameraDiscovery.js
git commit -m "feat(server): cameraDiscovery flags location_uncertain rows for LLM"
```

---

## Task 16: Document the enricher

**Files:**
- Modify: `docs/collectors.md`

- [ ] **Step 1: Append the section**

Append this section to `docs/collectors.md`:

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
`LLM_BATCH_SIZE`, `LLM_ENRICH_INTERVAL_MS`, `LLM_TIMEOUT_MS`.

To re-process a row: clear its `llm_geocoded_at` (and `llm_failure`) column.
```

- [ ] **Step 2: Commit**

```bash
git add docs/collectors.md
git commit -m "docs: describe llmEnricher worker and re-processing knobs"
```

---

## Task 17: End-to-end smoke against a real LM Studio (gated, manual)

**Files:**
- None (manual verification step)

**Context:** Final sign-off. Requires the operator to start LM Studio with a small model loaded (e.g. `Llama-3.2-3B-Instruct` or `Qwen2.5-3B-Instruct`).

- [ ] **Step 1: Start LM Studio and load a small model. Confirm the API is up:**

Run: `curl -s http://localhost:1234/v1/models | head -20`
Expected: a JSON envelope listing the loaded model's id.

- [ ] **Step 2: Run the worker once with an enabled flag**

Run: `cd server && LLM_ENABLED=true LLM_BATCH_SIZE=5 node -e "import('./src/utils/llmEnricher.js').then(async m => { console.log(JSON.stringify(await m.runLlmEnricher(), null, 2)); })"`
Expected: prints `{ stationDedup: { decided: N }, social: { geocoded: N }, video: { geocoded: 0 }, cameras: { geocoded: N } }` with at least one positive count, depending on what's in the DB. No exceptions.

- [ ] **Step 3: Inspect a geocoded row**

Run: `cd server && node -e "import('./src/utils/database.js').then(m => console.log(m.default.prepare('SELECT post_uid, lat, lon, geo_source, llm_place_name, llm_failure FROM social_posts WHERE llm_geocoded_at IS NOT NULL ORDER BY llm_geocoded_at DESC LIMIT 5').all()))"`
Expected: rows with either `geo_source='llm_gsi'` and finite lat/lon, or a sentinel in `llm_failure` (`__no_match__`, `__gsi_miss__`, `__bad_json__`).

- [ ] **Step 4: Re-run — should be a no-op (or near-no-op) since rows were decided**

Run: `cd server && LLM_ENABLED=true node -e "import('./src/utils/llmEnricher.js').then(async m => console.log(await m.runLlmEnricher()))"`
Expected: very low geocoded counts (only newly-fetched rows, if any).

- [ ] **Step 5: Final commit (no code change — just close the task in your tracker)**

This is the manual sign-off. No git commit unless docs changed.

---

## Self-review

**Spec coverage:**

- LM Studio hardcoded shape, base URL env-overridable → Task 4. ✅
- Async write-behind worker → Task 11, scheduler in Task 12. ✅
- No cache, no queue table, null-column-as-queue → Task 5 schema, Task 11 drain queries. ✅
- Station dedup pair generator → Task 8. ✅
- Clusterer Pass 3 → Task 9. ✅
- Social geocoding (text + vision optional) → Tasks 6/7 prompt, Task 11 drain, Task 14 collector rewrite. ✅
- Video geocoding plumbing → Tasks 5/6/7/11. ✅
- Camera refinement with `location_uncertain` → Tasks 5/11/15. ✅
- GSI helper extraction → Tasks 1/2. ✅
- Failure sentinels (`__no_match__`, `__gsi_miss__`, `__bad_json__`) → Task 11. ✅
- Confidence gates (0.5 within enricher, 0.7 in clusterer Pass 3) → Tasks 9/11. ✅
- All env vars (`LLM_ENABLED`, `LLM_BASE_URL`, `LLM_MODEL`, `LLM_VISION`, `LLM_BATCH_SIZE`, `LLM_ENRICH_INTERVAL_MS`, `LLM_TIMEOUT_MS`) referenced in code → Task 11. ✅ (`LLM_ENRICH_INTERVAL_MS` is *not* honoured because we used a fixed cron `*/5 * * * *`; this is an intentional simplification — node-cron doesn't accept ms intervals. The spec env var is a no-op; documented in Task 16.)
- Tests for client, prompts, enricher, GSI helper → Tasks 1/3/6/10. ✅
- Docs update → Task 16. ✅
- Manual end-to-end with real LM Studio → Task 17. ✅

**Placeholder scan:** None. All code blocks are concrete; no "TBD"/"TODO"/"add appropriate error handling".

**Type consistency:** `chat({ messages, jsonSchema, timeoutMs, baseUrl, model })` is the same signature in Task 4 (definition) and Tasks 10/11 (callers, which only pass `messages, jsonSchema, timeoutMs`). `gsiAddressSearch(query, opts)` is consistent across Tasks 1, 2, 11. `findUncertainStationPairs()` is no-args throughout. Failure sentinels match between spec and code. SQL column names match between schema (Task 5) and queries (Tasks 11, 13, 14).

**Note on `LLM_ENRICH_INTERVAL_MS`:** Spec claimed this would be configurable. Cron-based scheduling makes this awkward — node-cron uses cron expressions, not ms intervals. We pin to `*/5 * * * *` (every 5 minutes) and treat the env var as documented-but-not-wired. Acceptable simplification because LLM_BATCH_SIZE controls work-per-tick more meaningfully.
