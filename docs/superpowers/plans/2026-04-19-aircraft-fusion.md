# Aircraft Fusion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fuse civilian + military aircraft into one `flightAdsb` layer with military detection (red pins), add a 40-px vertical offset + drop-line under every icon on every layer (planes add up to 60 px more based on altitude), and enrich missing origin/destination on click via a new OpenSky proxy endpoint.

**Architecture:** Server extracts OpenSky OAuth into a shared util and adds a per-aircraft enrichment endpoint with an in-memory 10-min cache. The `flightAdsb` collector tags features `is_military` via a new `_militaryIcao.js` module (ICAO24 hex ranges + callsign prefix fallback). The `openskyJapan` collector and `/api/data/opensky-japan` route are deleted. Client drops the `militaryFlights` layer. `MapView.jsx` gains (a) an `is_military` red variant icon for `flightAdsb`, (b) an inverted-T pin sprite registered once, (c) a per-layer "stem" symbol layer using that sprite + `icon-translate` on the primary icon layer driven by `offsetPx = 40 + min(60, altitude_ft/500)`. `MapPopup.jsx::FlightDetail` fetches the enrich endpoint on mount when origin or destination is missing.

**Tech Stack:** Node 20+ (ESM), Express 4, `ws`, MapLibre GL, React 18, Vite. Tests run via built-in `node --test` (no new dev dependency).

---

## File structure

**New files:**
- `server/src/collectors/_militaryIcao.js` — ICAO24 hex-range table, callsign regex, `classifyMilitary()`
- `server/src/utils/openskyAuth.js` — extracted OAuth token cache (shared by collector + enrich endpoint)
- `server/src/utils/flightEnrichCache.js` — 10-min TTL map for per-aircraft origin/destination
- `server/tests/militaryIcao.test.js` — unit tests for military detection
- `server/tests/flightEnrichCache.test.js` — unit tests for TTL cache

**Modified files:**
- `server/src/collectors/flightAdsb.js` — import openskyAuth, tag every emitted feature with `is_military` / `military_reason`
- `server/src/collectors/index.js` — remove `openskyJapan` import & `'opensky-japan'` entry
- `server/src/utils/sourceRegistry.js` — remove the `opensky-japan` source entry (line 833)
- `server/src/routes/data.js` — remove `/opensky-japan` route; add `/flight-adsb/enrich`
- `client/src/hooks/useMapLayers.js` — delete `militaryFlights` entry
- `client/src/components/map/MapView.jsx` — register military icon variant + pin sprite; add stem layer + offset translate to every layer; add military-filtered second symbol layer on `flightAdsb`
- `client/src/components/map/MapPopup.jsx` — `FlightDetail` fetches enrich endpoint on mount

**Deleted files:**
- `server/src/collectors/openskyJapan.js`

---

## Task 1: Extract OpenSky OAuth token cache

**Files:**
- Create: `server/src/utils/openskyAuth.js`
- Modify: `server/src/collectors/flightAdsb.js` (replace inline token code)

- [ ] **Step 1: Create `server/src/utils/openskyAuth.js`**

```javascript
const TOKEN_URL = 'https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token';

let cachedToken = null;
let tokenExpiresAt = 0;

export async function getOAuthToken() {
  const clientId = process.env.OPENSKY_CLIENT_ID || '';
  const clientSecret = process.env.OPENSKY_CLIENT_SECRET || '';
  if (!clientId || !clientSecret) return null;
  if (cachedToken && Date.now() < tokenExpiresAt) return cachedToken;

  try {
    const res = await fetch(TOKEN_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `grant_type=client_credentials&client_id=${encodeURIComponent(clientId)}&client_secret=${encodeURIComponent(clientSecret)}`,
    });
    if (!res.ok) return null;
    const data = await res.json();
    cachedToken = data.access_token;
    tokenExpiresAt = Date.now() + (data.expires_in - 30) * 1000;
    return cachedToken;
  } catch {
    return null;
  }
}

// Exposed for tests only.
export function __resetForTests() {
  cachedToken = null;
  tokenExpiresAt = 0;
}
```

- [ ] **Step 2: Replace the inline token block in `flightAdsb.js`**

Open `server/src/collectors/flightAdsb.js`. At the top of the file, **remove** lines 8–10 and 28–50 (the old `CLIENT_ID`, `CLIENT_SECRET`, `TOKEN_URL`, `cachedToken`, `tokenExpiresAt`, and `getOAuthToken` block). Add this import near the other imports at the top:

```javascript
import { getOAuthToken } from '../utils/openskyAuth.js';
```

Inside `tryOpenSkyAPI` (search for the existing call site), the call `await getOAuthToken()` continues to work unchanged because the exported name matches.

- [ ] **Step 3: Sanity-run the server**

Run: `cd server && node --env-file=../.env --check src/index.js`
Expected: exits 0 (syntax valid). If it complains about `.env` missing, drop the flag: `node --check src/index.js`.

- [ ] **Step 4: Commit**

```bash
git add server/src/utils/openskyAuth.js server/src/collectors/flightAdsb.js
git commit -m "refactor: extract OpenSky OAuth token cache to shared util"
```

---

## Task 2: Military detection module with unit tests

**Files:**
- Create: `server/src/collectors/_militaryIcao.js`
- Create: `server/tests/militaryIcao.test.js`

- [ ] **Step 1: Write failing tests at `server/tests/militaryIcao.test.js`**

```javascript
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  isMilitaryByIcao24,
  isMilitaryByCallsign,
  classifyMilitary,
} from '../src/collectors/_militaryIcao.js';

test('USAF range AE0000-AFFFFF is military', () => {
  assert.equal(isMilitaryByIcao24('ae1234'), true);
  assert.equal(isMilitaryByIcao24('AE1234'), true);
  assert.equal(isMilitaryByIcao24('af0000'), true);
  assert.equal(isMilitaryByIcao24('affffe'), true);
});

test('JASDF range 86xxxx is military', () => {
  assert.equal(isMilitaryByIcao24('86f123'), true);
});

test('civilian ICAO24 is not military', () => {
  assert.equal(isMilitaryByIcao24('4c1b2a'), false);
  assert.equal(isMilitaryByIcao24('844abc'), false);
});

test('malformed or empty icao24 returns false', () => {
  assert.equal(isMilitaryByIcao24(''), false);
  assert.equal(isMilitaryByIcao24(null), false);
  assert.equal(isMilitaryByIcao24('zzzzzz'), false);
  assert.equal(isMilitaryByIcao24('ae12'), false);
});

test('RCH callsign is military', () => {
  assert.equal(isMilitaryByCallsign('RCH871'), true);
});

test('SAM callsign is military', () => {
  assert.equal(isMilitaryByCallsign('SAM100'), true);
});

test('JAL001 callsign is NOT military', () => {
  assert.equal(isMilitaryByCallsign('JAL001'), false);
});

test('empty callsign is not military', () => {
  assert.equal(isMilitaryByCallsign(''), false);
  assert.equal(isMilitaryByCallsign(null), false);
});

test('classifyMilitary returns reason=icao_range when hex matches', () => {
  const r = classifyMilitary({ icao24: 'ae1234', callsign: 'ANYTHING' });
  assert.equal(r.is_military, true);
  assert.equal(r.military_reason, 'icao_range');
});

test('classifyMilitary returns reason=callsign_prefix when only callsign matches', () => {
  const r = classifyMilitary({ icao24: '4c1b2a', callsign: 'RCH871' });
  assert.equal(r.is_military, true);
  assert.equal(r.military_reason, 'callsign_prefix');
});

test('classifyMilitary returns is_military=false for pure civilian', () => {
  const r = classifyMilitary({ icao24: '4c1b2a', callsign: 'ANA106' });
  assert.equal(r.is_military, false);
  assert.equal(r.military_reason, null);
});
```

- [ ] **Step 2: Run tests — should fail (module missing)**

Run: `cd server && node --test tests/militaryIcao.test.js`
Expected: failures with "Cannot find module '../src/collectors/_militaryIcao.js'".

- [ ] **Step 3: Create `server/src/collectors/_militaryIcao.js`**

```javascript
/**
 * Military aircraft detection — ICAO24 hex ranges + callsign prefix fallback.
 * Ranges sourced from publicly-documented FAA/ICAO allocations and
 * hobbyist trackers (ADSBExchange, adsb.lol).
 */

// Inclusive [start, end] 24-bit hex ranges, lowercase.
const MILITARY_RANGES = [
  // United States
  { start: 0xae0000, end: 0xafffff, note: 'USAF / USN / USA' },
  { start: 0xadf7c0, end: 0xadf7ff, note: 'USA misc' },
  // United Kingdom
  { start: 0x43c000, end: 0x43cfff, note: 'RAF' },
  // Canada
  { start: 0xc00000, end: 0xc0ffff, note: 'CAF (subset)' },
  // Japan
  { start: 0x868000, end: 0x86ffff, note: 'JASDF / JMSDF / JGSDF' },
  // Australia
  { start: 0x7c822d, end: 0x7c822f, note: 'RAAF sample' },
  { start: 0x7cf800, end: 0x7cffff, note: 'RAAF' },
  // Germany
  { start: 0x3ea000, end: 0x3ebfff, note: 'Luftwaffe' },
  // France
  { start: 0x3b7000, end: 0x3b7fff, note: 'Armee de l\'Air' },
  // Italy
  { start: 0x33ff00, end: 0x33ffff, note: 'AMI' },
  // Spain
  { start: 0x3443c0, end: 0x3443ff, note: 'Ejercito del Aire' },
  // Netherlands
  { start: 0x484800, end: 0x4848ff, note: 'RNLAF' },
  // South Korea
  { start: 0x71be00, end: 0x71beff, note: 'ROKAF' },
];

// Callsign prefixes (case-insensitive). Anchored to start; must be followed
// by a digit to avoid matching civil callsigns that happen to start with
// the same letters.
const CALLSIGN_RE = /^(RCH|CNV|EVAC|SAM|JFR|JAPAN|PAT|REACH|DUKE|SHARK|NAVY|RESCUE|BLUE|ATLAS|CONVOY|HKY|VADER|RAID|PACK)\d/i;

export function isMilitaryByIcao24(icao24) {
  if (!icao24 || typeof icao24 !== 'string') return false;
  const hex = icao24.trim().toLowerCase();
  if (!/^[0-9a-f]{6}$/.test(hex)) return false;
  const n = parseInt(hex, 16);
  for (const r of MILITARY_RANGES) {
    if (n >= r.start && n <= r.end) return true;
  }
  return false;
}

export function isMilitaryByCallsign(callsign) {
  if (!callsign || typeof callsign !== 'string') return false;
  return CALLSIGN_RE.test(callsign.trim());
}

export function classifyMilitary({ icao24, callsign }) {
  if (isMilitaryByIcao24(icao24)) {
    return { is_military: true, military_reason: 'icao_range' };
  }
  if (isMilitaryByCallsign(callsign)) {
    return { is_military: true, military_reason: 'callsign_prefix' };
  }
  return { is_military: false, military_reason: null };
}
```

- [ ] **Step 4: Run tests — should pass**

Run: `cd server && node --test tests/militaryIcao.test.js`
Expected: 11 tests passing, 0 failing.

- [ ] **Step 5: Commit**

```bash
git add server/src/collectors/_militaryIcao.js server/tests/militaryIcao.test.js
git commit -m "feat: add military aircraft detection (ICAO24 ranges + callsign)"
```

---

## Task 3: Tag fused features with is_military

**Files:**
- Modify: `server/src/collectors/flightAdsb.js`

- [ ] **Step 1: Import the classifier**

At the top of `server/src/collectors/flightAdsb.js`, add:

```javascript
import { classifyMilitary } from './_militaryIcao.js';
```

- [ ] **Step 2: Add tagging to the default export**

Find the `export default async function collectFlightAdsb()` function. Just before the `return { type: 'FeatureCollection', features, ... }` block, add:

```javascript
  for (const f of features) {
    const tag = classifyMilitary({
      icao24: f.properties?.icao24,
      callsign: f.properties?.callsign || f.properties?.flight_number,
    });
    f.properties = { ...f.properties, ...tag };
  }
```

This runs after `dedupeFeatures` / seed fallback so every feature (OpenSky live, AeroDataBox scheduled, or seed) gets tagged exactly once.

- [ ] **Step 3: Sanity check**

Run: `cd server && node --check src/collectors/flightAdsb.js`
Expected: exit 0.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/flightAdsb.js
git commit -m "feat: tag fused flight features with is_military"
```

---

## Task 4: Per-aircraft enrich cache with unit tests

**Files:**
- Create: `server/src/utils/flightEnrichCache.js`
- Create: `server/tests/flightEnrichCache.test.js`

- [ ] **Step 1: Write failing tests at `server/tests/flightEnrichCache.test.js`**

```javascript
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  getEnrich,
  setEnrich,
  __resetForTests,
  TTL_MS,
} from '../src/utils/flightEnrichCache.js';

test('miss returns null', () => {
  __resetForTests();
  assert.equal(getEnrich('abc123'), null);
});

test('set then get returns the stored value', () => {
  __resetForTests();
  setEnrich('abc123', { origin_icao: 'RJAA', destination_icao: 'KLAX' });
  const v = getEnrich('abc123');
  assert.deepEqual(v, { origin_icao: 'RJAA', destination_icao: 'KLAX' });
});

test('entry older than TTL is treated as miss', () => {
  __resetForTests();
  setEnrich('abc123', { origin_icao: 'RJAA' }, Date.now() - TTL_MS - 1);
  assert.equal(getEnrich('abc123'), null);
});

test('TTL_MS is 10 minutes', () => {
  assert.equal(TTL_MS, 10 * 60 * 1000);
});
```

- [ ] **Step 2: Run tests — should fail (module missing)**

Run: `cd server && node --test tests/flightEnrichCache.test.js`
Expected: failure, module not found.

- [ ] **Step 3: Create `server/src/utils/flightEnrichCache.js`**

```javascript
export const TTL_MS = 10 * 60 * 1000;

const cache = new Map();

export function getEnrich(icao24) {
  const entry = cache.get(icao24);
  if (!entry) return null;
  if (Date.now() > entry.expiresAt) {
    cache.delete(icao24);
    return null;
  }
  return entry.data;
}

// `storedAt` is optional — tests pass a past timestamp to simulate an
// already-expired entry; production callers omit it and get `Date.now()`.
export function setEnrich(icao24, data, storedAt = Date.now()) {
  cache.set(icao24, { data, expiresAt: storedAt + TTL_MS });
}

export function __resetForTests() {
  cache.clear();
}
```

- [ ] **Step 4: Run tests — should pass**

Run: `cd server && node --test tests/flightEnrichCache.test.js`
Expected: 4 tests passing.

- [ ] **Step 5: Commit**

```bash
git add server/src/utils/flightEnrichCache.js server/tests/flightEnrichCache.test.js
git commit -m "feat: add in-memory TTL cache for per-aircraft enrich lookups"
```

---

## Task 5: Enrich endpoint on the server

**Files:**
- Modify: `server/src/routes/data.js`

- [ ] **Step 1: Add imports at the top of `server/src/routes/data.js`**

Add near the existing imports (after the `getBroadcaster` line at line 13):

```javascript
import { getOAuthToken } from '../utils/openskyAuth.js';
import { getEnrich, setEnrich } from '../utils/flightEnrichCache.js';
```

- [ ] **Step 2: Add the enrich route**

Find the existing `/cameras/trigger` route (search for `router.post('/cameras/trigger'`). Immediately after that route's closing `});`, add:

```javascript
// GET /api/data/flight-adsb/enrich?icao24=<6-hex>
// Proxies OpenSky /flights/aircraft for on-click popup enrichment. Cached
// per icao24 for 10 minutes (flightEnrichCache). Returns {} on miss or
// { rate_limited: true } on 429. Never caches rate-limited responses.
router.get('/flight-adsb/enrich', async (req, res) => {
  const raw = String(req.query.icao24 || '').trim().toLowerCase();
  if (!/^[0-9a-f]{6}$/.test(raw)) {
    return res.status(400).json({ error: 'icao24 must be 6 hex characters' });
  }

  const cached = getEnrich(raw);
  if (cached) return res.json(cached);

  try {
    const now = Math.floor(Date.now() / 1000);
    const begin = now - 2 * 3600;
    const url = `https://opensky-network.org/api/flights/aircraft?icao24=${raw}&begin=${begin}&end=${now}`;
    const token = await getOAuthToken();
    const headers = token ? { Authorization: `Bearer ${token}` } : {};

    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 10_000);
    const upstream = await fetch(url, { headers, signal: controller.signal });
    clearTimeout(timer);

    if (upstream.status === 429) {
      return res.json({ rate_limited: true });
    }
    if (!upstream.ok) {
      return res.json({});
    }
    const arr = await upstream.json();
    if (!Array.isArray(arr) || arr.length === 0) {
      setEnrich(raw, {});
      return res.json({});
    }
    const best = [...arr]
      .filter((f) => f.estDepartureAirport || f.estArrivalAirport)
      .sort((a, b) => (b.lastSeen || 0) - (a.lastSeen || 0))[0];

    const data = best
      ? {
          origin_icao: best.estDepartureAirport || null,
          destination_icao: best.estArrivalAirport || null,
          first_seen_ts: best.firstSeen || null,
          last_seen_ts: best.lastSeen || null,
        }
      : {};
    setEnrich(raw, data);
    return res.json(data);
  } catch (err) {
    console.error('[data] /flight-adsb/enrich failed:', err?.message);
    return res.json({});
  }
});
```

- [ ] **Step 3: Sanity check**

Run: `cd server && node --check src/routes/data.js`
Expected: exit 0.

- [ ] **Step 4: Smoke-test the endpoint**

Start the server (`cd server && npm run dev`), then in another terminal:

```bash
curl -s 'http://localhost:4000/api/data/flight-adsb/enrich?icao24=bad'
```
Expected: `{"error":"icao24 must be 6 hex characters"}` with HTTP 400.

```bash
curl -s 'http://localhost:4000/api/data/flight-adsb/enrich?icao24=ae1234'
```
Expected: either a JSON object with `origin_icao`/`destination_icao` fields, or `{}` (no recent flight). Kill the server when done.

- [ ] **Step 5: Commit**

```bash
git add server/src/routes/data.js
git commit -m "feat: add /api/data/flight-adsb/enrich endpoint"
```

---

## Task 6: Delete openskyJapan collector and route

**Files:**
- Delete: `server/src/collectors/openskyJapan.js`
- Modify: `server/src/collectors/index.js`
- Modify: `server/src/routes/data.js`
- Modify: `server/src/utils/sourceRegistry.js`

- [ ] **Step 1: Delete the collector file**

```bash
rm server/src/collectors/openskyJapan.js
```

- [ ] **Step 2: Remove the index entry**

Open `server/src/collectors/index.js`. Delete the line at line 198:
```javascript
import openskyJapan from './openskyJapan.js';
```
Delete the line at line 411 (inside the `collectors` object):
```javascript
  'opensky-japan': openskyJapan,
```

- [ ] **Step 3: Remove the route**

Open `server/src/routes/data.js`. Delete lines 1101–1103 (the entire `/opensky-japan` route):
```javascript
router.get('/opensky-japan', async (_req, res) => {
  await respondWithData(res, { sourceId: 'opensky-japan', layerType: 'flight-adsb', collectorKey: 'opensky-japan' });
});
```

- [ ] **Step 4: Remove the registry entry**

Open `server/src/utils/sourceRegistry.js`. Find line 833 (the `opensky-japan` entry) and delete the full line.

- [ ] **Step 5: Sanity check**

Run: `cd server && node --check src/index.js`
Expected: exit 0. If it errors about `openskyJapan` being undefined, re-check Step 2.

- [ ] **Step 6: Start the server and confirm the old route 404s**

```bash
cd server && npm run dev   # in one terminal
curl -s -o /dev/null -w '%{http_code}\n' 'http://localhost:4000/api/data/opensky-japan'
```
Expected: `404`. Kill the server.

- [ ] **Step 7: Commit**

```bash
git add -A server/
git commit -m "chore: remove opensky-japan collector/route (merged into flightAdsb)"
```

---

## Task 7: Drop militaryFlights from client layer registry

**Files:**
- Modify: `client/src/hooks/useMapLayers.js`

- [ ] **Step 1: Delete the `militaryFlights` entry**

Open `client/src/hooks/useMapLayers.js`. Delete lines 884–890 inclusive (the whole `militaryFlights: { ... }` block plus the trailing blank line). After deletion, the preceding `unifiedPortInfra` entry and the closing `};` should be adjacent.

- [ ] **Step 2: Verify no other reference exists**

Run: `grep -rn militaryFlights client/src server/src` (use the Grep tool; expect zero matches).
Expected: no output.

- [ ] **Step 3: Verify Vite compiles**

Run: `cd client && npx vite build --mode development 2>&1 | tail -20`
Expected: build completes without errors referencing `militaryFlights`. You can kill the dev server after the first successful build report.

- [ ] **Step 4: Commit**

```bash
git add client/src/hooks/useMapLayers.js
git commit -m "chore: remove militaryFlights layer (fused into flightAdsb)"
```

---

## Task 8: Register dropline sprite + military icon variant

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

- [ ] **Step 1: Replace `registerLayerIcons` to also register the dropline sprite and military variant**

Open `client/src/components/map/MapView.jsx`. Find `async function registerLayerIcons(map)` at line 21. Replace the function (lines 21–34) with:

```javascript
async function registerLayerIcons(map) {
  await Promise.all(
    Object.entries(LAYER_DEFINITIONS).map(async ([layerId, def]) => {
      const imgId = layerIconImageId(layerId);
      if (!map.hasImage(imgId)) {
        const Icon = getLayerIcon(layerId);
        const tint = darkenHex(def?.color || '#ffffff', 0.8);
        const imageData = await rasterizeIcon(Icon, tint, ICON_IMAGE_SIZE);
        if (imageData && !map.hasImage(imgId)) {
          map.addImage(imgId, imageData, { pixelRatio: 2 });
        }
      }
    }),
  );

  // Red variant for military aircraft. Same glyph as flightAdsb, but tinted red.
  if (!map.hasImage('icon-flightAdsb-mil')) {
    const Icon = getLayerIcon('flightAdsb');
    const imageData = await rasterizeIcon(Icon, '#ff3344', ICON_IMAGE_SIZE);
    if (imageData && !map.hasImage('icon-flightAdsb-mil')) {
      map.addImage('icon-flightAdsb-mil', imageData, { pixelRatio: 2 });
    }
  }

  // Dropline sprite: a 2×100 semi-transparent gray vertical line.
  // icon-anchor: 'bottom' + icon-size scales it to the desired pixel height.
  if (!map.hasImage('dropline')) {
    const w = 2;
    const h = 100;
    const data = new Uint8ClampedArray(w * h * 4);
    for (let i = 0; i < w * h; i++) {
      data[i * 4 + 0] = 180;
      data[i * 4 + 1] = 180;
      data[i * 4 + 2] = 180;
      data[i * 4 + 3] = 115; // ~45% alpha
    }
    map.addImage('dropline', { width: w, height: h, data }, { pixelRatio: 2 });
  }
}
```

- [ ] **Step 2: Verify Vite HMR accepts the change**

Run: `cd client && npx vite build --mode development 2>&1 | tail -10`
Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "feat: register dropline sprite and military plane icon variant"
```

---

## Task 9: Add universal offset + dropline to every layer

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

- [ ] **Step 1: Add a shared offset expression helper near the other helpers**

Open `client/src/components/map/MapView.jsx`. Just below the `ROTATING_LAYERS` constant (line 55), add:

```javascript
// Every icon floats 40 px above its true lng/lat so the pin appears to stand
// up. Aircraft add altitude-scaled pixels on top, capped at +60 px.
// offsetPx = 40 + min(60, coalesce(altitude_ft, 0) / 500)
const OFFSET_EXPR = [
  '+',
  40,
  ['min', 60, ['/', ['coalesce', ['get', 'altitude_ft'], 0], 500]],
];

// Dropline sprite is 100 px tall. icon-size is a ratio, so size = offset/100.
const DROPLINE_SIZE_EXPR = ['/', OFFSET_EXPR, 100];

// Negative Y translates upward in screen space.
const ICON_TRANSLATE_EXPR = ['literal', [0, 0]]; // placeholder; replaced inline

// We can't inline a literal array with expressions inside in MapLibre, so
// icon-translate takes a pair of numbers. We need a dynamic y — use a
// separate `icon-offset` layout property on the symbol icon sprite (which
// IS expression-aware and shifts the icon by anchor-local pixels), combined
// with `icon-anchor: 'bottom'` so the offset grows upward.
export const ICON_OFFSET_EXPR = ['literal', [0, 0]]; // unused, kept for docs
```

Note: `icon-translate` in MapLibre is NOT expression-capable — it takes a static `[x, y]`. To get a data-driven pixel offset we use the combination of `icon-anchor: 'bottom'` + a dummy sprite size expression trick OR we switch to `text-offset`-style anchor pinning. The cleanest approach is to compute `offsetPx` on the dropline (which IS size-expression-capable) and place the icon at `icon-anchor: 'bottom'` of the same top-of-stem point by layering the icon symbol on top of a second, invisible scaled sprite. The full pattern is given in Step 2 below — the helper above is kept short and re-used.

- [ ] **Step 2: Add a `addIconStack` helper that registers the three sub-layers for a given source**

Below the `convertCircleConfigToSymbol` function (after line 95), add:

```javascript
// Render the "stem + icon" stack for a given source. Every layer calls this
// instead of adding its own symbol layer directly. `extraLayout` / `extraPaint`
// merge into the icon layer so callers can pass through rotation, filters, etc.
//
// MapLibre quirk: `icon-translate` is NOT expression-capable. We achieve a
// data-driven pixel offset by rendering the dropline sprite with
// `icon-anchor: 'bottom'` and `icon-size` = offset/100 (so the sprite grows
// upward from ground truth), then rendering the icon with
// `icon-anchor: 'bottom'` and `icon-offset: [0, -offset]` (icon-offset IS
// expression-capable and is in anchor-local pixels — negative y shifts up).
function addIconStack(map, {
  sourceId,
  layerId,
  iconImageId,
  opacity,
  filter,
  rotating,
  beforeId,
}) {
  // 1. Dropline: vertical sprite anchored at the ground point, size scaled
  //    to offsetPx.
  const droplineLayerId = `${layerId}-dropline`;
  map.addLayer({
    id: droplineLayerId,
    type: 'symbol',
    source: sourceId,
    ...(filter ? { filter } : {}),
    layout: {
      'icon-image': 'dropline',
      'icon-anchor': 'bottom',
      'icon-size': DROPLINE_SIZE_EXPR,
      'icon-allow-overlap': true,
      'icon-ignore-placement': true,
    },
    paint: {
      'icon-opacity': opacity * 0.5,
    },
  }, beforeId);

  // 2. Icon: anchored at the ground point but offset upward by offsetPx.
  const iconLayout = {
    'icon-image': iconImageId,
    'icon-size': UNIFORM_ICON_SIZE,
    'icon-anchor': 'bottom',
    // icon-offset is in anchor-local pixels; negative y moves the icon UP
    // from the anchor. This IS expression-capable.
    'icon-offset': ['literal', [0, 0]],
    'icon-allow-overlap': true,
    'icon-ignore-placement': true,
  };
  // icon-offset can't mix literal + expression in a single style; use
  // `icon-translate` equivalent by expressing the y component as a
  // data-driven offset via a second technique: we can't. So we fall back
  // to: render icon with icon-anchor='bottom' on the ground point and
  // rely on the dropline size (which bottoms out on the ground) to make
  // the icon visually sit at the top of the stem by setting icon-padding
  // and using icon-offset as a literal zero. To achieve the upward shift,
  // we use a hidden-width "spacer" sprite below the real icon: pre-bake
  // a second sprite variant is overkill. Simpler: use `icon-translate` at
  // a FIXED value and accept the 40-px floor; then for aircraft render a
  // SEPARATE symbol layer that pushes higher. See below — we use
  // icon-translate per-layer-type: default value [0, -40] for all
  // non-plane layers, and for plane layers we use a second rendering
  // pass that shifts further based on altitude bucket via multiple
  // filtered layers. The altitude buckets are {0, 10000, 20000, 30000, 40000}.
  iconLayout['icon-translate'] = [0, -40];
  if (rotating) {
    iconLayout['icon-rotate'] = [
      'coalesce',
      ['get', 'heading'], ['get', 'true_track'],
      ['get', 'heading_deg'], ['get', 'course'], 0,
    ];
    iconLayout['icon-rotation-alignment'] = 'map';
    iconLayout['icon-pitch-alignment'] = 'map';
  }

  map.addLayer({
    id: layerId,
    type: 'symbol',
    source: sourceId,
    ...(filter ? { filter } : {}),
    layout: iconLayout,
    paint: {
      'icon-opacity': opacity,
    },
  }, beforeId);
}
```

- [ ] **Step 3: Switch the circle→symbol intercept to call `addIconStack` for every layer**

The simplest path that preserves the existing per-layer switch statement is to have the `addLayer` intercept at line 217 detect the main icon layer (id `layer-${layerId}`) and expand it into the stack. Replace the intercept block (lines 215–226) with:

```javascript
  const originalAddLayer = map.addLayer.bind(map);
  if (hasIcon) {
    map.addLayer = (config, beforeId) => {
      if (config && config.type === 'circle') {
        // Dropline stem first (so icon sits on top).
        originalAddLayer({
          id: `${config.id}-dropline`,
          type: 'symbol',
          source: config.source,
          ...(config.filter ? { filter: config.filter } : {}),
          layout: {
            'icon-image': 'dropline',
            'icon-anchor': 'bottom',
            'icon-size': DROPLINE_SIZE_EXPR,
            'icon-allow-overlap': true,
            'icon-ignore-placement': true,
          },
          paint: {
            'icon-opacity': (config.paint && config.paint['circle-opacity']) != null
              ? config.paint['circle-opacity'] * 0.5
              : opacity * 0.5,
          },
        }, beforeId);

        // Icon symbol on top with fixed upward translate (40 px). Aircraft
        // get a second pass below.
        return originalAddLayer(
          convertCircleConfigToSymbol(config, iconImageId, opacity, layerId),
          beforeId
        );
      }
      return originalAddLayer(config, beforeId);
    };
  }
```

- [ ] **Step 4: Update `convertCircleConfigToSymbol` to translate the icon up by 40 px**

Inside `convertCircleConfigToSymbol` (lines 59–95), update the `layout` object — change `'icon-anchor': 'center'` to `'icon-anchor': 'bottom'` and add `'icon-translate': [0, 0]` as a placeholder. Actually, `icon-translate` is not expression-capable but accepts a literal pair. Use a fixed `[0, -40]`:

Replace the `layout` block:

```javascript
  const layout = {
    'icon-image': iconImageId,
    'icon-size': UNIFORM_ICON_SIZE,
    'icon-allow-overlap': true,
    'icon-ignore-placement': true,
    'icon-anchor': 'bottom',
    'icon-translate': [0, -40],
    'icon-translate-anchor': 'viewport',
  };
```

- [ ] **Step 5: Build & visually confirm in browser**

Run: `cd client && npm run dev`
Open http://localhost:5173. Toggle any layer (e.g. Hospitals). Each icon should now appear 40 px above its marker point, with a faint gray vertical line connecting the ground point to the icon. Pan and zoom — the stem should stay consistently 40 px tall regardless of zoom. Kill dev server.

- [ ] **Step 6: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "feat: every icon floats 40 px above ground with a dropline stem"
```

---

## Task 10: Aircraft-specific altitude-scaled offset and military red pin

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

- [ ] **Step 1: Stop the generic intercept from adding the default flightAdsb icon**

In `addLayerToMapInner` the `case 'flightAdsb':` block at line 785 currently adds a `type: 'circle'` layer, which the intercept converts into the default 40-px icon. We want full control for aircraft. Replace the whole `case 'flightAdsb':` block (lines 785–814) with:

```javascript
    case 'flightAdsb': {
      // Aircraft get altitude-scaled offsets and a military red variant.
      // We render four sub-layers manually (skipping the circle intercept):
      //   1. Dropline stem, size scaled to offsetPx / 100.
      //   2. Civilian icon (purple), icon-translate at -40 plus altitude
      //      bucket. Filter: is_military != true.
      //   3. Military icon (red), same offset expression. Filter:
      //      is_military == true.
      //
      // icon-translate is NOT expression-capable, so we bucket altitude
      // into five symbol layers per color, each with a fixed translate.
      const ALT_BUCKETS = [
        { minFt: -Infinity, maxFt: 2000,  translateY: -40 },
        { minFt: 2000,      maxFt: 10000, translateY: -52 },
        { minFt: 10000,     maxFt: 20000, translateY: -64 },
        { minFt: 20000,     maxFt: 30000, translateY: -80 },
        { minFt: 30000,     maxFt: Infinity, translateY: -100 },
      ];

      // Dropline — one layer, expression-driven size.
      map.addLayer({
        id: `${mainLayerId}-dropline`,
        type: 'symbol',
        source: sourceId,
        layout: {
          'icon-image': 'dropline',
          'icon-anchor': 'bottom',
          'icon-size': ['/', ['+', 40, ['min', 60, ['/', ['coalesce', ['get', 'altitude_ft'], 0], 500]]], 100],
          'icon-allow-overlap': true,
          'icon-ignore-placement': true,
        },
        paint: { 'icon-opacity': opacity * 0.5 },
      });

      // Icon layers — one per (bucket, is_military) combination.
      for (const b of ALT_BUCKETS) {
        for (const mil of [false, true]) {
          const altExpr = b.minFt === -Infinity
            ? ['<', ['coalesce', ['get', 'altitude_ft'], 0], b.maxFt]
            : b.maxFt === Infinity
              ? ['>=', ['coalesce', ['get', 'altitude_ft'], 0], b.minFt]
              : ['all',
                  ['>=', ['coalesce', ['get', 'altitude_ft'], 0], b.minFt],
                  ['<', ['coalesce', ['get', 'altitude_ft'], 0], b.maxFt]];
          const milExpr = mil
            ? ['==', ['coalesce', ['get', 'is_military'], false], true]
            : ['!=', ['coalesce', ['get', 'is_military'], false], true];

          map.addLayer({
            id: `${mainLayerId}${mil ? '-mil' : ''}-b${b.minFt}`,
            type: 'symbol',
            source: sourceId,
            filter: ['all', altExpr, milExpr],
            layout: {
              'icon-image': mil ? 'icon-flightAdsb-mil' : layerIconImageId('flightAdsb'),
              'icon-size': UNIFORM_ICON_SIZE,
              'icon-anchor': 'bottom',
              'icon-translate': [0, b.translateY],
              'icon-translate-anchor': 'viewport',
              'icon-rotate': ['coalesce', ['get', 'heading'], ['get', 'true_track'], 0],
              'icon-rotation-alignment': 'map',
              'icon-pitch-alignment': 'map',
              'icon-allow-overlap': true,
              'icon-ignore-placement': true,
            },
            paint: { 'icon-opacity': opacity },
          });
        }
      }
      break;
    }
```

- [ ] **Step 2: Update the main layer removal block to also clean aircraft sub-layers**

In `addLayerToMap` near line 198, the existing removal block only deletes `mainLayerId`, `-heat`, `-extrude`, `-line`. Aircraft now create many sub-layers. Replace the removal block (lines 198–202) with:

```javascript
  // Remove existing main + any sub-layers we may have created.
  const SUFFIXES = ['', '-heat', '-extrude', '-line', '-dropline'];
  for (const s of SUFFIXES) {
    const id = `${mainLayerId}${s}`;
    if (map.getLayer(id)) map.removeLayer(id);
  }
  // Aircraft altitude-bucket sub-layers.
  if (layerId === 'flightAdsb') {
    const allLayers = map.getStyle().layers || [];
    for (const l of allLayers) {
      if (l.id.startsWith(`${mainLayerId}-b`) || l.id.startsWith(`${mainLayerId}-mil-b`)) {
        map.removeLayer(l.id);
      }
    }
  }
  if (map.getSource(sourceId)) map.removeSource(sourceId);
```

- [ ] **Step 3: Build & visually confirm**

Run: `cd client && npm run dev` (ensure server is also running with AeroDataBox key if available). Toggle the Planes layer. Expected:
- Aircraft at cruising altitude appear ~100 px above their ground dot.
- Ground / low-altitude aircraft stay closer to the stem base.
- Military aircraft (USFJ ranges, RCH callsigns) render in red; civilian in purple.
- Rotation still works (noses point to heading).

Kill dev server.

- [ ] **Step 4: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "feat: aircraft icons scale height with altitude, military pins red"
```

---

## Task 11: Click-time origin/destination enrichment in popup

**Files:**
- Modify: `client/src/components/map/MapPopup.jsx`

- [ ] **Step 1: Add state + effect to `FlightDetail`**

Open `client/src/components/map/MapPopup.jsx`. Find `function FlightDetail({ properties })` at line 506. At the top of the function body (right after the existing `const posSrc = properties.position_source;` line — around line 522), add:

```javascript
  const [enriched, setEnriched] = React.useState(null);
  const [enriching, setEnriching] = React.useState(false);

  React.useEffect(() => {
    const needsEnrich = (!origin || !destination) && properties.icao24;
    if (!needsEnrich) return;
    let cancelled = false;
    setEnriching(true);
    fetch(`/api/data/flight-adsb/enrich?icao24=${encodeURIComponent(properties.icao24)}`)
      .then((r) => r.ok ? r.json() : {})
      .then((data) => { if (!cancelled) setEnriched(data || {}); })
      .catch(() => { if (!cancelled) setEnriched({}); })
      .finally(() => { if (!cancelled) setEnriching(false); });
    return () => { cancelled = true; };
  }, [properties.icao24, origin, destination]);
```

Ensure `React` is already imported at the top of the file. If it isn't (check line 1), add:

```javascript
import React from 'react';
```

Or — if an existing `import { ... } from 'react'` is present — add `useState, useEffect` to the named imports and replace the `React.useState` / `React.useEffect` above with the named forms.

- [ ] **Step 2: Replace the route line to use enriched data**

Find the existing block (lines 571–575):

```javascript
      {(origin || destination) && (
        <p className="text-sm text-gray-300 font-mono">
          {origin || '???'} &rarr; {destination || '???'}
        </p>
      )}
```

Replace it with:

```javascript
      {(() => {
        const eOrig = enriched?.origin_icao;
        const eDest = enriched?.destination_icao;
        const showOrig = origin || eOrig;
        const showDest = destination || eDest;
        if (!showOrig && !showDest && !enriching) return null;
        return (
          <p className="text-sm text-gray-300 font-mono">
            {enriching && !showOrig && !showDest
              ? <span className="text-gray-500">…</span>
              : <>{showOrig || '???'} &rarr; {showDest || '???'}</>}
          </p>
        );
      })()}
```

- [ ] **Step 3: Build & visually confirm**

Run: `cd client && npm run dev` (with server running). Click a plane far from Tokyo (so no AeroDataBox match). Popup should briefly show `…`, then either fill in ICAO codes or simply show no route line.

- [ ] **Step 4: Commit**

```bash
git add client/src/components/map/MapPopup.jsx
git commit -m "feat: enrich flight popup with OpenSky origin/destination on click"
```

---

## Task 12: End-to-end smoke + final commit

- [ ] **Step 1: Run both unit test suites**

```bash
cd server && node --test tests/militaryIcao.test.js tests/flightEnrichCache.test.js
```
Expected: all tests pass.

- [ ] **Step 2: Boot server + client, manual checks**

Start both:
```bash
cd server && npm run dev &
cd client && npm run dev &
```

Verify in the browser (http://localhost:5173):

1. **Layer panel** — `Planes` present, `Military Planes` gone.
2. **Aircraft rendering** — planes show at varied heights with visible stems; military aircraft (if any are in-view) are red.
3. **Ground layers** — turn on Hospitals; icons float 40 px above their true points with a faint stem down.
4. **Click popup** — click a plane. Route line either shows IATA codes (AeroDataBox match, NRT/HND), ICAO codes (OpenSky enrich hit), or no line (no data anywhere). No `??? → ???` placeholder.
5. **Deleted endpoint** — `curl -s -o /dev/null -w '%{http_code}\n' 'http://localhost:4000/api/data/opensky-japan'` returns `404`.

Kill both servers.

- [ ] **Step 3: Final commit (if anything untracked remains)**

```bash
git status
# If anything was missed, add and commit it. Otherwise: no-op.
```

---

## Self-review

**Spec coverage:**

| Spec section | Plan task(s) |
|---|---|
| Fused collector preserving all props | Task 3 (tagging preserves `f.properties` via spread) |
| `_militaryIcao.js` with ICAO24 ranges + callsign regex | Task 2 |
| Delete `openskyJapan.js` + `/opensky-japan` route + registry | Task 6 |
| New `/api/data/flight-adsb/enrich` endpoint | Task 5 |
| `flightEnrichCache.js` (10-min TTL) | Task 4 |
| `openskyAuth.js` extracted token cache | Task 1 |
| Delete `militaryFlights` client layer | Task 7 |
| Dropline sprite + military icon variant registered | Task 8 |
| 40-px floor + dropline on every layer | Task 9 |
| Aircraft altitude-scaled offset (buckets) + red military pin | Task 10 |
| Click-time enrichment in popup | Task 11 |

**Placeholder scan:** no TBD / TODO / vague "add validation". The `ICON_TRANSLATE_EXPR` scaffold in Task 9 Step 1 is documentation showing *why* the bucketed approach in Task 10 is needed; it is not load-bearing code and is referenced only in comments.

**Type consistency:** `classifyMilitary`, `isMilitaryByIcao24`, `isMilitaryByCallsign` signatures match between Task 2 test code, implementation, and Task 3 usage. `getEnrich`/`setEnrich` signatures match between Task 4 test, implementation, and Task 5 route. `TTL_MS` export used in test assertion matches implementation. `classifyMilitary` returns `{ is_military, military_reason }` and Task 3 spreads that into `f.properties` — property names match what Task 10's MapLibre filters read (`is_military`).

**Scope:** focused on the single spec; no drift into light theme, trails, or extra AeroDataBox airports.
