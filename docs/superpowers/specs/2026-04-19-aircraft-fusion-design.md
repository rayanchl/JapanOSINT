# Aircraft layer fusion, military tagging, drop-lines & click enrichment

## Context

The map has two disjoint aircraft layers today: `flightAdsb` (OpenSky live
positions fused with AeroDataBox scheduled flights for NRT+HND) and
`militaryFlights` (a thin wrapper around OpenSky with no enrichment). Users
have to toggle both to see "all planes," and military aircraft are visually
indistinguishable from civilian except by layer toggle. Separately, plane
icons sit exactly on their ground `[lng, lat]` with no altitude indication,
so a cruising 777 and a parked Cessna look identical on the map.

This spec merges both layers into one data stream, tags military aircraft
by ICAO24 hex range + callsign prefix (rendered as red pins), adds an
altitude-scaled pixel offset with a drop-line to ground for every icon on
every layer (40 px floor, plane altitude adds up to 60 px), and enriches
missing `origin`/`destination` on popup click via a new OpenSky
per-aircraft endpoint.

## Scope

In scope:

1. Fuse `flightAdsb` + `openskyJapan` into one collector. Keep all props.
2. Delete the `militaryFlights` layer toggle and the `openskyJapan.js`
   collector + `/api/data/opensky-japan` route.
3. Tag aircraft as military via ICAO24 hex ranges and callsign prefix
   fallback. Render military in red.
4. Render every icon on every layer with a 40-px vertical offset from its
   ground `[lng, lat]`, with a thin drop-line between the ground point
   and the icon. Aircraft get additional altitude-scaled offset (0–60 px,
   capped at 40,000 ft cruise).
5. On click, if the plane popup has missing `origin` or `destination`,
   fetch from OpenSky `/flights/aircraft` via a new server proxy endpoint
   that caches per-aircraft results for 10 minutes.

Out of scope (explicitly):

- Aircraft trails / historical position paths (dropped during brainstorm).
- Light theme (separate brainstorm).
- Extending AeroDataBox to additional airports (deferred in favor of
  click-time enrichment).
- ICAO↔IATA airport code conversion.

## Architecture

### Server

**Fused collector — `server/src/collectors/flightAdsb.js`** (existing
file, extended):

- Fetch stack unchanged: OpenSky `/states/all` Japan bbox + AeroDataBox
  NRT/HND scheduled flights.
- `dedupeFeatures` preserves live OpenSky position/altitude/speed/heading
  AND AeroDataBox `origin`/`destination`/`airline`/`aircraft_type`/
  `scheduled_time` when callsigns match (already correct, will be
  audited).
- **New post-processing step**: tag every feature with
  `properties.is_military: boolean` and `properties.military_reason:
  'icao_range' | 'callsign_prefix' | null`.

**New file — `server/src/collectors/_militaryIcao.js`**:

- Exports `isMilitaryByIcao24(hex: string): boolean`.
- Exports `isMilitaryByCallsign(callsign: string): boolean`.
- Exports `classifyMilitary({ icao24, callsign }): { is_military, military_reason }`.
- Static hex-range table covering USAF (`AE0000–AFFFFF`), USN, USMC,
  USCG, USA, JASDF, JMSDF, JGSDF, RAF, RAAF, and other allied air arms.
  ~30 range entries.
- Callsign regex: `/^(RCH|CNV|EVAC|SAM|JFR|JAPAN\d|PAT\d|REACH|DUKE|SHARK|NAVY|RESCUE|BLUE|ATLAS)/i`.

**Deletions**:

- `server/src/collectors/openskyJapan.js` — remove.
- `/api/data/opensky-japan` route — remove from `server/src/routes/data.js`.
- Entry for `opensky-japan` in `server/src/collectors/index.js` — remove.
- Any sourceRegistry entry for `opensky-japan` — remove.

**New endpoint — `GET /api/data/flight-adsb/enrich?icao24=XXXXXX`** in
`server/src/routes/data.js`:

- Validates `icao24` is a 6-hex-char string, lowercased.
- Checks in-memory cache first (10-min TTL).
- On miss: calls
  `https://opensky-network.org/api/flights/aircraft?icao24=<hex>&begin=<now-7200>&end=<now>`,
  reusing the existing OAuth token cache in `flightAdsb.js`
  (export the token getter from that file, or move it to a shared
  `server/src/utils/openskyAuth.js` — the latter is cleaner and is the
  approach we'll take).
- Takes the most-recent returned flight with non-null
  `estDepartureAirport` / `estArrivalAirport`.
- Response: `{ origin_icao, destination_icao, first_seen_ts, last_seen_ts }`,
  or `{}` on no-match, or `{ rate_limited: true }` on HTTP 429.
- Cache key = `icao24`, value = `{ data, expiresAt }`. Rate-limited
  responses are NOT cached.

**New util — `server/src/utils/flightEnrichCache.js`**:

- Tiny module-level `Map<string, { data, expiresAt }>`.
- `get(icao24)`, `set(icao24, data)`, `TTL_MS = 10 * 60 * 1000`.
- No eviction beyond TTL (map grows to at most a few thousand entries
  per day — acceptable).

**New util — `server/src/utils/openskyAuth.js`**:

- Extracted from `flightAdsb.js`: `getOAuthToken()` and token cache.
- Used by both `flightAdsb.js` collector and the new enrich endpoint.

### Client

**`client/src/hooks/useMapLayers.js`**:

- Delete the `militaryFlights` entry (~lines 884–890).
- Keep `flightAdsb` unchanged. Same endpoint, same color.

**`client/src/components/map/MapView.jsx`** — this file grows significantly
with the icon-offset + drop-line logic. The additions are scoped to layer
registration; keep them in the existing file (it's already the layer
rendering home). If the file crosses ~800 lines as a result, factor the
new rendering stack into `client/src/components/map/iconStack.js`.

- **Register a new base image** `'dropline'` at map init: a 2×100 PNG,
  semi-transparent gray (`rgba(180,180,180,0.45)`). Rasterized once in
  the existing `registerLayerIcons`-style helper. Sprite height 100 px
  matches the maximum icon offset (40 + 60).
- **For EVERY layer** (not just aircraft), replace the single symbol
  layer with a two-layer stack. Define once:
  `offsetPx = 40 + min(60, coalesce(altitude_ft, 0) / 500)`
  Both sub-layers use the same expression so the icon always sits at the
  top of the stem.
  1. `<layerId>-dropline` — symbol layer using the `dropline` image with
     `icon-anchor: 'bottom'`, `icon-size: offsetPx / 100` (scales the
     100-px sprite to the exact pixel height needed).
  2. `<layerId>` — existing symbol layer, with `icon-translate: [0,
     -offsetPx]` (negated because MapLibre Y is down).
  In practice these are MapLibre expression objects; the `offsetPx`
  pseudo-variable is inlined into each expression (no shared variable
  support in the style spec).
- **Aircraft-only: second symbol layer for military** — register a red
  (`#ff3344`) rasterized variant of the plane icon as
  `icon-flightAdsb-mil`. Add a sibling symbol layer filtered by
  `['==', ['get', 'is_military'], true]`. The default civilian layer
  gets the opposite filter.
- **Ground dot**: only rendered for aircraft (where icon position differs
  from true `[lng, lat]`). A `circle` layer before the dropline, radius
  2.5, color `#888`. For non-aircraft layers, the icon IS at ground —
  no ground dot needed (their foot of the stem is the true position).

**`client/src/components/map/MapPopup.jsx`** — `FlightDetail`:

- Add local state: `[enriched, setEnriched] = useState(null)`.
- `useEffect` on mount: if `(!origin || !destination) && icao24`, fire
  `fetch('/api/data/flight-adsb/enrich?icao24=' + icao24)`, store result.
- While pending and origin/destination both missing: render `… → …`.
- On success, render `origin || enriched.origin_icao` → `destination ||
  enriched.destination_icao`.
- On failure or empty response: behave exactly like today (existing
  `(origin || destination)` guard at line 571 hides the route line if
  both remain null).

## Data flow

```
OpenSky /states/all ─┐
AeroDataBox NRT/HND ─┴─> flightAdsb collector
                           │
                           ├─ dedupeFeatures (merge by callsign)
                           │
                           └─ tag is_military (icao24 range + callsign)
                                     │
                                     └─> /api/data/flight-adsb
                                                │
                                                └─> client filters into
                                                    {civilian symbol layer,
                                                     military symbol layer (red)}
                                                    both over the same GeoJSON source

click on plane popup:
  FlightDetail mounts ─> if missing origin|destination:
                           GET /api/data/flight-adsb/enrich?icao24=X
                             │
                             ├─ cache hit → return
                             └─ cache miss → OpenSky /flights/aircraft
                                               ├─ found → cache + return
                                               └─ 429 → return {rate_limited}
```

## Error handling

- **OpenSky down / empty**: existing fallback to seed features. Seed
  features never have `icao24` → `is_military` always false, enrichment
  never fires. Fine.
- **AeroDataBox key missing**: existing behavior — zero scheduled
  features, OpenSky passes through with no origin/destination fields.
  Click-time enrichment now fills that gap.
- **Enrich endpoint 429**: client gets `{ rate_limited: true }`, treats
  it as a miss. No retry.
- **Enrich endpoint network error**: client silently drops, keeps
  existing behavior (no route line).
- **Altitude null** (on-ground AeroDataBox synthetic features): offset
  expression uses `coalesce` → 0 → icon sits on ground dot with just the
  40-px floor. Correct.
- **`icon-translate` not supported on basemap with pitch**: our map is
  top-down, pitch is disabled in `MapView.jsx`. If pitch is ever enabled,
  the pixel-space trick will visually break — acceptable for now since
  it's not a current feature.

## Testing

- **Unit**: `_militaryIcao.js` — table of known hex / callsign examples
  (AE1234 USAF → military, 867A12 JASDF → military, 4C1B2A random
  civilian → not, RCH123 callsign → military, JAL001 → not).
- **Integration**:
  - Run the fused collector against live OpenSky + AeroDataBox (or
    mocked fixtures). Verify `is_military` flag is set, AeroDataBox
    matches preserve origin/destination, merged features retain live
    altitude.
  - `/api/data/flight-adsb/enrich` with a known icao24 returns ICAO
    codes; second call within 10 min is served from cache (no second
    HTTP hit — assert via spy).
- **Manual smoke**:
  - Toggle Aircraft layer → planes render with drop-lines, cruise
    aircraft sit higher on screen than ground aircraft.
  - Military aircraft (e.g. USFJ callsigns) render in red.
  - Click a non-NRT/HND plane → popup shows `… → …` briefly, then
    either a resolved route or the existing metadata if OpenSky has
    nothing. No `??? → ???`.
  - Toggle a ground layer (e.g. Hospitals) → icons are offset 40 px up
    with a faint stem down to the ground position.
  - `/api/data/opensky-japan` returns 404.

## Critical files

| Path | Role |
|------|------|
| `server/src/collectors/flightAdsb.js` | Fused collector, extended with military tagging |
| `server/src/collectors/_militaryIcao.js` | **New** — ICAO24 ranges + callsign regex |
| `server/src/collectors/openskyJapan.js` | **Delete** |
| `server/src/collectors/index.js` | Remove `opensky-japan` entry |
| `server/src/routes/data.js` | Remove `/opensky-japan`; add `/flight-adsb/enrich` |
| `server/src/utils/openskyAuth.js` | **New** — extracted OAuth token cache |
| `server/src/utils/flightEnrichCache.js` | **New** — 10-min TTL cache |
| `server/src/utils/sourceRegistry.js` | Drop `opensky-japan` source if present |
| `client/src/hooks/useMapLayers.js` | Delete `militaryFlights` layer definition |
| `client/src/components/map/MapView.jsx` | Icon stack: dropline sprite, offset, military red variant, ground dot |
| `client/src/components/map/MapPopup.jsx` | `FlightDetail` on-click enrichment fetch |
| `client/src/components/map/iconStack.js` | **New (optional)** — factor icon stack out if MapView grows >800 lines |

## Rollout

One PR, all pieces. The `militaryFlights` layer's deletion is an atomic
UX change that needs the fusion to land together.
