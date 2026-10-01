# LLM-Assisted Station Dedup, Social Geocoding, and Video Geocoding

**Date:** 2026-04-26
**Status:** Design — pending user approval

## Goals

1. Use a local LLM (LM Studio, OpenAI-compatible HTTP) to resolve **uncertain station-merge decisions** the existing string-fingerprint clusterer can't make confidently.
2. Use the same LLM to **extract place names from ungeocoded social posts** (Twitter / Mastodon / Wikipedia hits without coords today), then resolve those names to coordinates via the existing `gsiGeocode` API.
3. Use the same LLM to **extract place names from video-feed records** (YouTube titles, channel names, thumbnails, descriptions) and geocode them the same way.
4. Optionally use a **vision-capable model** (gated by `LLM_VISION=true`) to consider attached images / thumbnails when the text alone is ambiguous.

## Non-goals

- No new LLM-backend abstraction. LM Studio's OpenAI-compatible shape is hardcoded; only the base URL is env-overridable.
- No prompt/answer cache. The DB row's own `llm_geocoded_at` column is the persistence; re-running the LLM on a row means clearing that column.
- No queue / job table. Rows whose `llm_geocoded_at IS NULL` *are* the queue.
- No new YouTube or richer social collectors land in this spec. We add the storage table and the worker plumbing; populating those tables fully is follow-up work. We do, however, wire the existing `socialMedia.js` / `twitterGeo.js` to persist their *ungeocoded* posts so the worker has real input on day one.
- No multi-model routing. One text model, plus an optional vision model controlled by a flag.
- No re-geocoding of rows already produced by the LLM. Once `llm_geocoded_at` is set, that row is final until the column is manually nulled.

## Constraints discovered during exploration

- `cameras.lat / cameras.lon` and `transport_stations.lat / transport_stations.lon` are `NOT NULL`. We can't represent "ungeocoded" as `lat IS NULL` on these tables. For cameras we add an `llm_place_name` nullable column and let `cameraDiscovery.js` flag uncertain rows in `properties.location_uncertain`. For station dedup we don't touch lat/lon at all — we write pair-level merge decisions to a side table that the clusterer reads.
- The existing `socialMedia.js` and `twitterGeo.js` collectors today *only* emit features whose location they could already resolve (Wikipedia geosearch hits / Mastodon posts whose text contains a hardcoded place name from `JAPAN_PLACES`). They drop everything else. To give the LLM something to enrich, both collectors must start persisting all their fetched items into a new `social_posts` table — geocoded or not — and the existing in-memory FeatureCollection return becomes a SELECT over that table filtered on `lat IS NOT NULL`.

## Architecture

### Single LM Studio client

`server/src/utils/llmClient.js` (~80 lines). One exported async function:

```js
chat({ messages, jsonSchema, timeoutMs = 30_000 }) → object | null
```

- POSTs to `${LLM_BASE_URL ?? 'http://localhost:1234'}/v1/chat/completions`.
- Sends `model: process.env.LLM_MODEL` (defaults to `'local-model'`, which LM Studio accepts).
- Always sets `response_format: { type: 'json_schema', json_schema: { schema: jsonSchema, strict: true } }` so the model returns parseable JSON.
- `messages` is the standard OpenAI shape. Vision callers append `{ type: 'image_url', image_url: { url } }` content parts; the client doesn't care.
- Returns the parsed JSON object on success, or `null` on timeout, connection error, non-2xx, or unparseable body. Never throws.
- No retries. The enricher's next tick retries.

### Async write-behind worker

`server/src/utils/llmEnricher.js`. Registered in `scheduler.js` as a single tick that runs every `LLM_ENRICH_INTERVAL_MS` (default `300_000` = 5 minutes). The tick:

1. Skips entirely if `process.env.LLM_ENABLED !== 'true'`.
2. Calls three drain functions in series, each bounded to `LLM_BATCH_SIZE` (default 50) rows per tick:
   - `enrichStationDedup()`
   - `enrichSocialGeocode()`
   - `enrichVideoGeocode()` (no-op until a video collector exists, but the function and table are in place)
3. Each drain catches its own errors and logs without stopping the next.

The worker is a regular collector-shaped module — it integrates with the existing scheduler the same way `gtfsRtCatalogueRunner` does.

### Job 1 — Station dedup

The existing `stationClusterer.js` runs two passes today: wikidata identity, then name-fingerprint + spatial. After Pass 2, plenty of *near-miss* pairs remain — same physical station with operator-specific renaming, e.g. `"霞ヶ関"` (Tokyo Metro) vs `"国会議事堂前"` (Tokyo Metro, connected concourse), or romaji vs kana variants Levenshtein can't bridge.

#### Candidate generation

A new helper `findUncertainStationPairs()` in `stationClusterer.js`:

- Reuses the same coarse cell index Pass 2 builds.
- Yields pairs `(i, j)` where:
  - both are mode `train` or `subway`,
  - distance ≤ `SPATIAL_RADIUS_M` (150 m),
  - fingerprints differ AND Levenshtein > 2 (i.e. the existing logic would NOT merge them),
  - operator strings differ OR `name_ja` is null on either side (cheap "interesting" filter to keep the candidate set small).
- Returns rows: `{ uid_a, uid_b, name_a, name_ja_a, operator_a, line_a, mode_a, lat_a, lon_a, name_b, name_ja_b, operator_b, line_b, mode_b, lat_b, lon_b, dist_m }`.

#### LLM call

The worker filters out pairs already present in `llm_station_merges` (any `decided_at`, regardless of `same`) before building prompts — those have been answered and don't need re-asking. For each remaining pair, build a prompt of the shape:

```
System: You are a Japanese rail station identity matcher. Given two station
records that are within 150 metres of each other, decide whether they refer
to the same physical station/interchange. Two records describe the same
station if a passenger could transfer between them without leaving a paid
area or by walking through a connected concourse. Different stations on
overlapping platforms (rare) are NOT the same station.

User: Station A:
  name: "霞ケ関"
  name_ja: "霞ケ関"
  operator: "Tokyo Metro"
  line: "Marunouchi"
  mode: subway

Station B:
  name: "Kasumigaseki"
  name_ja: "霞ヶ関"
  operator: "Tokyo Metro"
  line: "Hibiya"
  mode: subway

Distance: 38 m
```

JSON schema for the response:

```json
{
  "type": "object",
  "properties": {
    "same_station": { "type": "boolean" },
    "confidence":   { "type": "number", "minimum": 0, "maximum": 1 },
    "reason":       { "type": "string", "maxLength": 200 }
  },
  "required": ["same_station", "confidence"]
}
```

#### Persistence

New table:

```sql
CREATE TABLE IF NOT EXISTS llm_station_merges (
  uid_a       TEXT NOT NULL,
  uid_b       TEXT NOT NULL,
  same        INTEGER NOT NULL,         -- 0 or 1
  confidence  REAL NOT NULL,
  reason      TEXT,
  decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (uid_a, uid_b)
);
```

Pair ordering: `uid_a < uid_b` lexicographically, so each pair is stored once.

The clusterer's third pass (added to `stationClusterer.js`):

```js
// Pass 3: LLM-decided merges.
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

The 0.7 threshold lives in code, not in the prompt — gives us one knob to tune without re-running the model.

### Job 2 — Social post geocoding

#### New table

```sql
CREATE TABLE IF NOT EXISTS social_posts (
  post_uid          TEXT PRIMARY KEY,
  platform          TEXT NOT NULL,        -- 'twitter' | 'mastodon' | 'wikipedia' | ...
  author            TEXT,
  text              TEXT,
  title             TEXT,
  url               TEXT,
  media_urls        TEXT,                 -- JSON array of image/video URLs
  language          TEXT,
  fetched_at        TEXT NOT NULL DEFAULT (datetime('now')),
  posted_at         TEXT,

  lat               REAL,                 -- nullable: ungeocoded posts allowed
  lon               REAL,
  geo_source        TEXT,                 -- 'native_geo' (Twitter API geo) | 'place_match' (substring hit against JAPAN_PLACES) | 'llm_gsi' (LLM extracted name → GSI resolved) | null

  llm_place_name    TEXT,
  llm_geocoded_at   TEXT,
  llm_failure       TEXT,                 -- '__no_match__' | '__bad_json__' | '__gsi_miss__' | null

  properties        TEXT NOT NULL DEFAULT '{}'
);
CREATE INDEX IF NOT EXISTS idx_social_posts_geo ON social_posts(lat, lon);
CREATE INDEX IF NOT EXISTS idx_social_posts_pending
  ON social_posts(llm_geocoded_at) WHERE llm_geocoded_at IS NULL;
```

#### Collector changes

- `twitterGeo.js` and `socialMedia.js` switch from "return FeatureCollection" to "INSERT into `social_posts`, then SELECT geocoded rows back as FeatureCollection". Native-geo Twitter posts get `geo_source='native_geo'`; Mastodon posts whose text matches a hardcoded place name get `geo_source='place_match'`; everything else is inserted with `lat=lon=NULL` for the worker to handle.
- Existing `_meta.live` / `recordCount` semantics preserved — they reflect rows with `lat IS NOT NULL` only.

#### Drain query

```sql
SELECT post_uid, platform, author, text, title, media_urls, language
FROM social_posts
WHERE llm_geocoded_at IS NULL
  AND (text IS NOT NULL OR title IS NOT NULL)
ORDER BY fetched_at DESC
LIMIT ?
```

#### LLM call

```
System: You extract Japanese place names from social media posts. Return the
single most specific Japanese place mentioned in the post — a neighbourhood,
station, landmark, or address. Prefer the place where the author is, not
places merely mentioned in conversation. If no place is mentioned, return
null.

User: Platform: twitter
Author: @somehandle
Text: "今日の渋谷スクランブル混みすぎ、もう動けん"
Title: (none)
```

Vision content parts (image_url) are appended for each entry in `media_urls` when `LLM_VISION=true`, capped at 2 images, each ≤ 4 MB (HEAD-checked).

JSON schema:

```json
{
  "type": "object",
  "properties": {
    "place":       { "type": ["string", "null"], "maxLength": 100 },
    "confidence":  { "type": "number", "minimum": 0, "maximum": 1 }
  },
  "required": ["place", "confidence"]
}
```

#### Resolve via GSI

If `place != null && confidence >= 0.5`, the worker calls GSI's address-search (extracted from `gsiGeocode.js` into a reusable `gsiAddressSearch(query)` helper). On a hit, write:

```
UPDATE social_posts
SET lat = ?, lon = ?, geo_source = 'llm_gsi',
    llm_place_name = ?, llm_geocoded_at = datetime('now'), llm_failure = NULL
WHERE post_uid = ?
```

Failure modes (each writes `llm_geocoded_at = now` to stop re-asking, plus a sentinel in `llm_failure`):

- LLM said `place=null` → `llm_failure='__no_match__'`.
- Bad JSON / null response from `llmClient.chat()` → `llm_failure='__bad_json__'`.
- GSI returned no result → `llm_failure='__gsi_miss__'`, `llm_place_name` filled with what the LLM said.

To re-process a row: `UPDATE social_posts SET llm_geocoded_at = NULL, llm_failure = NULL WHERE …`.

### Job 3 — Video geocoding

Schema-compatible twin of `social_posts`:

```sql
CREATE TABLE IF NOT EXISTS video_items (
  video_uid         TEXT PRIMARY KEY,
  platform          TEXT NOT NULL,        -- 'youtube' | 'niconico' | 'twitch' | ...
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
CREATE INDEX IF NOT EXISTS idx_video_items_geo ON video_items(lat, lon);
CREATE INDEX IF NOT EXISTS idx_video_items_pending
  ON video_items(llm_geocoded_at) WHERE llm_geocoded_at IS NULL;
```

Drain logic is identical to social, except the prompt feeds title + description + channel name and the vision payload is the thumbnail (one image only).

The drain function is wired in but no collector populates `video_items` in this spec. That keeps the LLM plumbing testable end-to-end (insert a fake row, watch the worker geocode it) without coupling to a YouTube API integration.

### Camera location refinement

Existing `cameras` table gets two added nullable columns via a migration in `database.js`:

```sql
ALTER TABLE cameras ADD COLUMN llm_place_name  TEXT;
ALTER TABLE cameras ADD COLUMN llm_geocoded_at TEXT;
```

Drain query:

```sql
SELECT camera_uid, name, properties
FROM cameras
WHERE llm_geocoded_at IS NULL
  AND json_extract(properties, '$.location_uncertain') = 1
LIMIT ?
```

Same flow as social: LLM returns place-name → GSI → `UPDATE cameras SET lat=?, lon=?, llm_place_name=?, llm_geocoded_at=…`. Note this **overwrites** the existing lat/lon (which was a guess from `cameraDiscovery.js`); we keep the original in `properties.original_lat / original_lon` before overwriting.

`cameraDiscovery.js` is updated to mark uncertain rows by writing `properties.location_uncertain = 1`. This spec defines the contract; choosing which discovery channels are "uncertain" is up to that file's existing logic.

## Data flow diagram

```
collector tick
   │
   ├─ socialMedia.js / twitterGeo.js  ─►  INSERT social_posts (lat may be NULL)
   ├─ cameraDiscovery.js              ─►  INSERT cameras with location_uncertain=1
   ├─ stationClusterer.js (Pass 2)    ─►  emits uncertain pair list
   ▼
SQLite: rows with llm_geocoded_at IS NULL

scheduler tick (every 5 min)
   │
   ▼
llmEnricher
   │
   ├─ enrichStationDedup() ─► LM Studio ─► llm_station_merges
   ├─ enrichSocialGeocode() ─► LM Studio ─► gsiAddressSearch ─► UPDATE social_posts
   ├─ enrichVideoGeocode()  ─► LM Studio ─► gsiAddressSearch ─► UPDATE video_items
   └─ enrichCameras()       ─► LM Studio ─► gsiAddressSearch ─► UPDATE cameras
                                                (overwrites uncertain lat/lon)

clusterer tick (already exists, runs less often)
   │
   ▼
stationClusterer
   │
   ├─ Pass 1: wikidata
   ├─ Pass 2: name fingerprint + 150 m
   └─ Pass 3: read llm_station_merges where same=1 AND confidence>=0.7
```

## Error handling

| Failure | Behaviour |
|---|---|
| `LLM_ENABLED != 'true'` | Worker tick is a no-op. No errors logged. |
| LM Studio unreachable / timeout | `chat()` returns `null`. Worker logs once per tick: `[llmEnricher] LM Studio at $URL unreachable, skipping tick`. Rows stay pending; next tick retries. |
| LM Studio returns non-JSON / schema-violating JSON | Row marked with `llm_failure='__bad_json__'`, `llm_geocoded_at=now`. Stops the row looping forever. Manually clear to retry. |
| LLM returns `place=null` or low confidence | Row marked `llm_failure='__no_match__'`, `llm_geocoded_at=now`. |
| GSI returns no match | Row marked `llm_failure='__gsi_miss__'`, `llm_place_name` preserved, `lat/lon` stay null, `llm_geocoded_at=now`. |
| Vision call when `LLM_VISION=false` | Code path silently drops `image_url` parts; never an error. |
| Vision image >4 MB or HEAD fails | Image dropped; remaining parts (text + other images) still sent. |
| Worker tick crash mid-batch | One drain function's failure logs and continues to the next. The whole tick runs inside `try/catch` so the next interval still fires. |

## Configuration

Env vars (all optional, defaults in code):

| Var | Default | Purpose |
|---|---|---|
| `LLM_ENABLED` | `'false'` | Master kill-switch. |
| `LLM_BASE_URL` | `http://localhost:1234` | LM Studio endpoint. |
| `LLM_MODEL` | `'local-model'` | Identifier sent to LM Studio. |
| `LLM_VISION` | `'false'` | If `'true'`, append image content parts. |
| `LLM_BATCH_SIZE` | `50` | Rows per drain function per tick. |
| `LLM_ENRICH_INTERVAL_MS` | `300_000` | Worker tick interval. |
| `LLM_TIMEOUT_MS` | `30_000` | Per-call timeout. |

## Testing

- **`llmClient` unit tests** — Node `http.createServer` stub returning canned OpenAI envelopes. Cases: success, timeout (server hangs), 500, malformed JSON body, schema-violating JSON, vision content shape (verify the `image_url` parts make it through unchanged).
- **Prompt-builder unit tests** — Each enricher's prompt-building helper is a pure function. Snapshot tests against fixture rows (one canonical pair, one mastodon post, one twitter post with media).
- **Enricher integration test (gated by `LLM_TEST=1`)** — Real LM Studio at localhost. Three known cases:
  - Dedup pair: `霞ケ関 (Marunouchi)` ↔ `Kasumigaseki (Hibiya)` → expect `same=true`.
  - Social post: `"今日の渋谷スクランブル混みすぎ"` → expect `place ≈ '渋谷'`, GSI resolves to lat/lon near 35.66, 139.70.
  - Ambiguous: `"今日寒いね"` → expect `place=null`.
- **End-to-end smoke** — Insert a fake `social_posts` row, run one worker tick, assert the row got `lat/lon/llm_place_name/llm_geocoded_at` filled. Skipped without `LLM_TEST=1`.

## Open questions

None. All scope decisions made during brainstorming are reflected above.

## File touch list (informational, for plan-writing)

- `server/src/utils/llmClient.js` — new
- `server/src/utils/llmEnricher.js` — new
- `server/src/utils/gsiAddressSearch.js` — new (extracted from `gsiGeocode.js`)
- `server/src/utils/database.js` — schema additions: `social_posts`, `video_items`, `llm_station_merges`, two columns on `cameras`
- `server/src/utils/scheduler.js` — register `llmEnricher` tick
- `server/src/utils/stationClusterer.js` — Pass 3 reading `llm_station_merges`, plus `findUncertainStationPairs()` helper exposed for the worker
- `server/src/collectors/socialMedia.js` — persist into `social_posts`, return geocoded rows from DB
- `server/src/collectors/twitterGeo.js` — same
- `server/src/collectors/cameraDiscovery.js` — set `properties.location_uncertain = 1` on uncertain rows (logic-level only, no schema change beyond columns above)
- `server/package.json` — no new dependencies (uses built-in `fetch`)
- `docs/collectors.md` — short paragraph on the LLM enricher

## Rollout

1. Land the schema + `llmClient.js` + `llmEnricher.js` skeleton with `LLM_ENABLED=false` default. Worker is registered but no-op. No behaviour change.
2. Land the social/twitter collector rewrites (persist all posts; FeatureCollection now sourced from DB). Behaviour identical for users since ungeocoded posts were already dropped.
3. Land the LLM enricher logic. Test locally with LM Studio + `LLM_ENABLED=true`. No remote rollout — single-developer / single-machine deployment, the operator turns it on when their LM Studio is running.
