import { Router } from 'express';
import crypto from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { getSourceById } from '../utils/database.js';
import { getCameraByUid, getDiscoveryFeed } from '../utils/cameraStore.js';
import { collectors } from '../collectors/index.js';
import { captureSnapshot } from '../utils/screenshot.js';
import { isRunInFlight, runCameraDiscovery } from '../utils/cameraRunner.js';
import { withCollectorRun, annotateLastHit, getBroadcaster } from '../utils/collectorTap.js';
import { getOAuthToken } from '../utils/openskyAuth.js';
import { getEnrich, setEnrich } from '../utils/flightEnrichCache.js';
import { fetchDamLive } from '../utils/mlitDamLive.js';
import { getSnapshot as getFlightsSnapshot } from '../utils/planeAdsbPoller.js';
import {
  getCached, setCached, getTtlMs,
} from '../utils/collectorCache.js';
import { mirrorCollectorOutput } from '../utils/collectorMirror.js';
import { selectGeoFeatures } from '../utils/intelStore.js';
import {
  broadcastLayerWorkStarted, broadcastLayerWorkFinished,
} from '../utils/layerEvents.js';
import { getTemporalForLayer } from '../utils/layerTemporal.js';

const router = Router();

// Time-slider query parsing. iOS / web clients hitting /api/data/:layer can
// pass ?at=<iso>&window=<seconds> to request a historical snapshot. Parsed
// once here and stashed on res.locals so respondWithData (and any sibling
// route) can reach the values without threading req through every helper.
router.use((req, res, next) => {
  const atRaw = req.query.at;
  const windowRaw = req.query.window;
  if (atRaw == null && windowRaw == null) {
    res.locals.timeQuery = { at: null, windowSec: null };
    return next();
  }
  const at = atRaw ? new Date(String(atRaw)) : null;
  if (atRaw && Number.isNaN(at?.getTime())) {
    return res.status(400).json({ error: 'invalid_at' });
  }
  const windowSec = windowRaw != null ? Number(windowRaw) : null;
  if (at && !(Number.isFinite(windowSec) && windowSec > 0)) {
    return res.status(400).json({ error: 'missing_window' });
  }
  res.locals.timeQuery = { at, windowSec };
  next();
});

// ── Camera snapshot cache ───────────────────────────────────────────────────
// Lazy, one-time (per URL, 24h TTL) screenshots of embed-blocked webcams.
// Stored as JPEG in server/data/snapshots/<sha1>.jpg.
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SNAPSHOT_DIR = path.resolve(__dirname, '..', '..', 'data', 'snapshots');
const SNAPSHOT_TTL_MS = 24 * 60 * 60 * 1000;

const SNAPSHOT_ALLOWLIST = new Set([
  'skylinewebcams.com',
  'www.skylinewebcams.com',
  'embed.skylinewebcams.com',
  'webcamtaxi.com',
  'www.webcamtaxi.com',
  'geocam.ru',
  'www.geocam.ru',
  'worldcams.tv',
  'www.worldcams.tv',
  'webcamera24.com',
  'www.webcamera24.com',
  'camstreamer.com',
  'www.camstreamer.com',
  'earthcam.com',
  'www.earthcam.com',
  'livecam.asia',
  'www.livecam.asia',
  'windy.com',
  'www.windy.com',
  'insecam.org',
  'www.insecam.org',
  'worldcam.eu',
  'fr.worldcam.eu',
  'de.worldcam.eu',
  'es.worldcam.eu',
  'www.worldcam.eu',
  'worldcam.pl',
  'www.worldcam.pl',
]);

// In-flight capture dedup so two popup opens for the same URL don't launch
// two captures in parallel.
const inflight = new Map();

function snapshotPath(url) {
  const hash = crypto.createHash('sha1').update(url).digest('hex');
  return path.join(SNAPSHOT_DIR, `${hash}.jpg`);
}

async function ensureSnapshotDir() {
  await fs.mkdir(SNAPSHOT_DIR, { recursive: true });
}

async function readFreshCache(file) {
  try {
    const stat = await fs.stat(file);
    if (Date.now() - stat.mtimeMs < SNAPSHOT_TTL_MS) return await fs.readFile(file);
  } catch { /* miss */ }
  return null;
}

async function captureAndCache(url, file) {
  if (inflight.has(url)) return inflight.get(url);
  const p = (async () => {
    const buf = await captureSnapshot(url);
    if (buf) {
      await ensureSnapshotDir();
      await fs.writeFile(file, buf);
    }
    return buf;
  })().finally(() => { inflight.delete(url); });
  inflight.set(url, p);
  return p;
}

/**
 * Normalise whatever the collector returned into the canonical FC envelope.
 * The canonical shape is { source, fetchedAt, recordCount, live,
 * description } + any optional additive fields a collector chose to emit
 * (live_source, by_type, db_total, etc). Additive fields pass through so
 * downstream consumers keep them.
 *
 * We tolerate rare camel/snake drift (record_count, timestamp) as a fallback
 * when a future collector slips up, but every collector in the tree today
 * already emits the canonical keys — this is belt-and-braces, not a feature.
 */
function normaliseFc(data, collectorKey) {
  const features = Array.isArray(data?.features) ? data.features
    : (Array.isArray(data) ? data : []);
  const m = (data && typeof data === 'object') ? (data._meta || {}) : {};
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      ...m,
      source: m.source ?? collectorKey,
      fetchedAt: m.fetchedAt ?? m.timestamp ?? new Date().toISOString(),
      recordCount: features.length,
      live: (m.live != null) ? !!m.live : (features.length > 0),
      description: m.description ?? null,
    },
  };
}

/**
 * Build the map FC for a layer by reading from intel_items (the polymorphic
 * master). Returns null if the source has no rows there yet — caller falls
 * back to the legacy collector_cache FC.
 *
 * The cached FC is still the TTL gate: when fresh, we skip running the
 * collector and serve from intel_items. When stale, we run + mirror + read.
 */
function buildFcFromIntel(sourceId, { extraMeta = {}, at = null, windowSec = null, field, fallbackField } = {}) {
  const fc = selectGeoFeatures({ sourceId, at, windowSec, field, fallbackField });
  if (!fc || fc.features.length === 0) return null;
  return {
    ...fc,
    _meta: {
      source: sourceId,
      fetchedAt: new Date().toISOString(),
      recordCount: fc.features.length,
      live: at == null,
      description: null,
      served_from: 'intel_items',
      ...(at ? { replay: { at: at instanceof Date ? at.toISOString() : String(at), window: windowSec } } : {}),
      ...extraMeta,
    },
  };
}

const EMPTY_FC = Object.freeze({
  type: 'FeatureCollection',
  features: [],
  _meta: { source: null, fetchedAt: null, recordCount: 0, live: false, description: 'empty' },
});

function emptyReplayFc(sourceId, reason, { at = null, windowSec = null } = {}) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source: sourceId,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      live: false,
      description: reason,
      ...(at ? { replay: { at: at instanceof Date ? at.toISOString() : String(at), window: windowSec } } : {}),
    },
  };
}

/**
 * Helper: run the collector live for this source, or return an empty
 * FeatureCollection with status info if no collector is registered.
 *
 * Post-Phase-B flow:
 *  1. If TTL is fresh (collector_cache hit): respond from intel_items (or
 *     fall back to the cached FC if intel_items has nothing yet).
 *  2. If TTL is stale: run the collector → mirror → respond from intel_items
 *     (or fall back to the freshly-normalised FC).
 */
async function respondWithData(res, { sourceId, layerType, collectorKey }) {
  try {
    // ── Time-slider short-circuit ──────────────────────────────────────────
    // When the caller passed ?at=<iso>&window=<seconds>, bypass the live
    // collector flow and serve a historical snapshot from intel_items.
    // - liveOnly layers (e.g. unified-flights): no archive exists, return empty.
    // - static layers (transport reference tables): no per-row event time,
    //   fall through to the normal flow so the user sees current state.
    // - temporal layers (everything else): query intel_items with the
    //   trailing time window applied to the layer's declared event-time
    //   column (COALESCE-fallback to fetched_at handled inside selectGeoFeatures).
    const { at, windowSec } = res.locals.timeQuery || {};
    if (at) {
      const t = getTemporalForLayer(layerType);
      if (t?.liveOnly) {
        return res.json(emptyReplayFc(sourceId, 'liveOnly layer hidden in replay', { at, windowSec }));
      }
      if (t) {
        const intelFc = buildFcFromIntel(sourceId, {
          at, windowSec,
          field: t.field, fallbackField: t.fallbackField,
          extraMeta: { cache_status: 'skip', age_ms: 0 },
        });
        return res.json(intelFc || emptyReplayFc(sourceId, 'no rows in window', { at, windowSec }));
      }
      // t == null → static; fall through to live data path below.
    }

    const collector = collectorKey ? collectors[collectorKey] : null;
    if (!collector) {
      const source = getSourceById(sourceId);
      // Even with no collector registered, intel_items may have rows that
      // landed via /api/intel/sources/:id/run or another path. Serve those.
      const intelFc = buildFcFromIntel(sourceId, { extraMeta: { cache_status: 'miss', age_ms: 0 } });
      if (intelFc) return res.json(intelFc);
      return res.json({
        type: 'FeatureCollection',
        features: [],
        _meta: {
          source: sourceId,
          fetchedAt: new Date().toISOString(),
          recordCount: 0,
          live: false,
          description: 'No collector registered for this source.',
          cache_status: 'miss',
          age_ms: 0,
          status: source?.status ?? 'unknown',
        },
      });
    }

    // 1. Cache hit short-circuit — TTL is fresh, no need to re-run the
    // collector. Serve the geocoded subset from intel_items if available;
    // legacy collector_cache FC as fallback (covers sources that haven't
    // been mirrored since the Phase A migration).
    const cached = getCached(collectorKey);
    if (cached) {
      const intelFc = buildFcFromIntel(sourceId, {
        extraMeta: {
          cache_status: 'hit',
          age_ms: cached.ageMs,
          ttl_ms: getTtlMs(collectorKey),
        },
      });
      if (intelFc) {
        broadcastLayerWorkFinished({
          layerId: layerType, collectorKey, sourceId,
          durationMs: 0, recordCount: intelFc.features.length, cacheStatus: 'hit',
        });
        return res.json(intelFc);
      }
      const fc = cached.fc;
      fc._meta = {
        ...(fc._meta || {}),
        cache_status: 'hit',
        age_ms: cached.ageMs,
        ttl_ms: getTtlMs(collectorKey),
        served_from: 'collector_cache',
      };
      broadcastLayerWorkFinished({
        layerId: layerType,
        collectorKey,
        sourceId,
        durationMs: 0,
        recordCount: fc.features?.length ?? 0,
        cacheStatus: 'hit',
      });
      return res.json(fc);
    }

    // 2. Cache miss — invoke the collector
    broadcastLayerWorkStarted({ layerId: layerType, collectorKey, sourceId });
    const startedAt = Date.now();
    let data;
    try {
      data = await withCollectorRun(
        collectorKey,
        () => collector(),
        { trigger: 'on-demand' },
      );
    } catch (err) {
      broadcastLayerWorkFinished({
        layerId: layerType,
        collectorKey,
        sourceId,
        durationMs: Date.now() - startedAt,
        recordCount: 0,
        cacheStatus: 'error',
      });
      throw err;
    }

    // ── Mirror to intel_items (polymorphic master) ────────────────────
    // Every collector run side-effects into intel_items: FC features, intel
    // envelope items, and hybrid (FC + intel) all flow through one bridge.
    // The map response continues to come from collector_cache as before;
    // the mirror is what makes intel_items the unified store for the Intel
    // tab. Errors are logged but never break the map response.
    const mirrorFetchedAt = data?.meta?.fetchedAt || data?._meta?.fetchedAt || new Date().toISOString();
    let mirrorCounts = null;
    try {
      mirrorCounts = await mirrorCollectorOutput(data, sourceId, mirrorFetchedAt);
    } catch (err) {
      console.warn(`[data] mirror failed for ${sourceId}:`, err?.message);
    }

    // ── Intel branch ──────────────────────────────────────────────────
    // Non-spatial collectors return { kind:'intel', items, meta }. The
    // mirror above already upserted them; here we just shape an empty FC
    // for legacy map clients that hit /api/data/:id.
    if (data && data.kind === 'intel') {
      const items = Array.isArray(data.items) ? data.items : [];
      const recordCount = items.length;
      annotateLastHit({ record_count: recordCount, data_type: 'intel' });
      const emptyFc = {
        type: 'FeatureCollection',
        features: [],
        _meta: {
          source: sourceId,
          fetchedAt: mirrorFetchedAt,
          recordCount,
          live: recordCount > 0,
          description: data?.meta?.description ?? 'Migrated to /api/intel/items',
          migrated_to_intel: true,
          intel_endpoint: `/api/intel/items?source=${encodeURIComponent(sourceId)}`,
          cache_status: 'miss',
          age_ms: 0,
          ingested: mirrorCounts?.intel || null,
        },
      };
      broadcastLayerWorkFinished({
        layerId: layerType,
        collectorKey,
        sourceId,
        durationMs: Date.now() - startedAt,
        recordCount,
        cacheStatus: 'miss',
      });
      return res.json(emptyFc);
    }

    const fc = normaliseFc(data, collectorKey);
    const recordCount = fc.features.length;
    annotateLastHit({ record_count: recordCount });

    // Persist for the source's TTL window — the cache is now a marker, not
    // the source of truth. We keep storing the FC so older callers (and the
    // fallback path above) still work for sources that haven't been mirrored.
    const ttlMs = getTtlMs(collectorKey);
    setCached(collectorKey, fc, ttlMs);

    // Prefer the intel_items reconstruction: it carries the polymorphic
    // master's properties (record_type, sub_source_id, geom_source, …) which
    // the cached FC doesn't. Falls back to the normalised collector output
    // when the mirror produced nothing for this source.
    const intelFc = buildFcFromIntel(sourceId, {
      extraMeta: { cache_status: 'miss', age_ms: 0, ttl_ms: ttlMs },
    });
    const out = intelFc || (() => {
      fc._meta = { ...fc._meta, cache_status: 'miss', age_ms: 0, ttl_ms: ttlMs, served_from: 'collector_cache' };
      return fc;
    })();

    broadcastLayerWorkFinished({
      layerId: layerType,
      collectorKey,
      sourceId,
      durationMs: Date.now() - startedAt,
      recordCount: out.features.length,
      cacheStatus: 'miss',
    });

    return res.json(out);
  } catch (err) {
    console.error(`[data] Error fetching ${sourceId}:`, err.message);
    res.status(500).json({ error: `Failed to fetch ${layerType} data` });
  }
}

// GET /api/data/earthquake
router.get('/earthquake', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-earthquake',
    layerType: 'earthquake',
    collectorKey: 'jma-earthquake',
  });
});

// GET /api/data/gdelt — on-demand fetch from GDELT 2.0 (Japan, 1d)
router.get('/gdelt', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'gdelt-events',
    layerType: 'gdelt',
    collectorKey: 'gdelt-events',
  });
});

// GET /api/data/weather
router.get('/weather', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-weather',
    layerType: 'weather',
    collectorKey: 'jma-weather',
  });
});

// GET /api/data/transport
router.get('/transport', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'odpt-train',
    layerType: 'transport',
    collectorKey: 'odpt-transport',
  });
});

// GET /api/data/air-quality
router.get('/air-quality', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'soramame',
    layerType: 'air-quality',
    collectorKey: 'soramame',
  });
});

// GET /api/data/radiation
router.get('/radiation', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nra-radiation',
    layerType: 'radiation',
    collectorKey: 'nra-radiation',
  });
});

// GET /api/data/cameras — flows through the standard respondWithData path so
// it gets the shared TTL cache + layer_work_* telemetry. The registered
// `cameras` collector reads cameraStore's DB (populated by the hourly
// camera-discovery sweep) and returns a conformant FC.
router.get('/cameras', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'cameras',
    layerType: 'cameras',
    collectorKey: 'cameras',
  });
});

// GET /api/data/cameras/discovery-feed — backfill for the Camera Discovery
// thread. Returns rows in the same shape the WS hook already consumes so the
// client can seed its event list without a live run firing first.
router.get('/cameras/discovery-feed', (req, res) => {
  try {
    const limit  = req.query.limit  != null ? Number(req.query.limit) : 500;
    const cursor = req.query.cursor ? String(req.query.cursor) : null;
    const channel = req.query.channel ? String(req.query.channel) : null;
    const { events, cursor: nextCursor } = getDiscoveryFeed({ limit, cursor, channel });
    res.json({ events, cursor: nextCursor });
  } catch (err) {
    console.error('[data] discovery-feed failed:', err.message);
    res.status(500).json({ error: 'Failed to load discovery feed' });
  }
});

// POST /api/data/cameras/trigger — kick a camera-discovery run on demand
// (e.g. when the user toggles the Cameras layer on). The runner already
// dedupes via _inflightRun, so repeated calls during an active run are safe.
router.post('/cameras/trigger', (_req, res) => {
  if (isRunInFlight()) {
    return res.json({ started: false, already_running: true });
  }
  const wsServer = getBroadcaster();
  withCollectorRun('cameraDiscovery', () => runCameraDiscovery(wsServer), { trigger: 'manual' })
    .catch((err) => {
      console.error('[data] manual camera run failed:', err?.message);
    });
  res.json({ started: true, already_running: false });
});

// GET /api/data/flight-adsb/enrich?icao24=<6-hex>
// Proxies OpenSky /flights/aircraft for on-click popup enrichment. Cached
// per icao24 for 10 minutes (flightEnrichCache). Returns {} on miss or
// { rate_limited: true } on 429. Rate-limited responses are NOT cached.
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
    let upstream;
    try {
      upstream = await fetch(url, { headers, signal: controller.signal });
    } finally {
      clearTimeout(timer);
    }

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

// ── Unified transport endpoints ────────────────────────────────────────────
// These now flow through the standard respondWithData path — the registered
// `unified-*` collectors read from the persistent transport_* SQLite tables
// populated by the background transportRunner sweep. Caching + layer_work_*
// telemetry come for free; the DB read itself is a millisecond-order lookup.
router.get('/unified-trains', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-trains', layerType: 'unified-trains', collectorKey: 'unified-trains' });
});
router.get('/unified-subways', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-subways', layerType: 'unified-subways', collectorKey: 'unified-subways' });
});
router.get('/unified-buses', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-buses', layerType: 'unified-buses', collectorKey: 'unified-buses' });
});
router.get('/unified-ais-ships', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-ais-ships', layerType: 'unified-ais-ships', collectorKey: 'unified-ais-ships' });
});
router.get('/unified-port-infra', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-port-infra', layerType: 'unified-port-infra', collectorKey: 'unified-port-infra' });
});
router.get('/unified-stations', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-stations', layerType: 'unified-stations', collectorKey: 'unified-stations' });
});
router.get('/unified-airports', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-airports', layerType: 'unified-airports', collectorKey: 'unified-airports' });
});

// Direct passthrough to the in-memory snapshot maintained by
// planeAdsbPoller (OpenSky live + AeroDataBox scheduled, deduped). Skips the
// transport_store DB indirection that the unified-* template uses — flight
// positions are too ephemeral to round-trip through SQLite, and the pre-fused
// snapshot already has everything iOS needs.
router.get('/unified-flights', (_req, res) => {
  const { at, windowSec } = res.locals.timeQuery || {};
  if (at) return res.json(emptyReplayFc('unified-flights', 'liveOnly layer hidden in replay', { at, windowSec }));
  res.json(getFlightsSnapshot());
});

router.get('/unified-station-footprints', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-station-footprints', layerType: 'unified-station-footprints', collectorKey: 'unified-station-footprints' });
});

// GET /api/data/cameras/snapshot?url=<encoded> — on-demand JPEG screenshot
// of an embed-blocked webcam page, cached for 24h.
router.get('/cameras/snapshot', async (req, res) => {
  const raw = req.query.url;
  if (typeof raw !== 'string' || !raw) {
    return res.status(400).json({ error: 'url query param required' });
  }
  let parsed;
  try {
    parsed = new URL(raw);
  } catch {
    return res.status(400).json({ error: 'invalid url' });
  }
  if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
    return res.status(400).json({ error: 'only http/https allowed' });
  }
  if (!SNAPSHOT_ALLOWLIST.has(parsed.hostname.toLowerCase())) {
    return res.status(400).json({ error: 'hostname not allowed' });
  }

  const file = snapshotPath(parsed.href);
  const sendBuf = (buf) => {
    res.setHeader('Content-Type', 'image/jpeg');
    res.setHeader('Cache-Control', 'public, max-age=86400');
    res.send(buf);
  };

  const fresh = await readFreshCache(file);
  if (fresh) return sendBuf(fresh);

  try {
    const buf = await captureAndCache(parsed.href, file);
    if (!buf) return res.status(502).json({ error: 'capture failed' });
    return sendBuf(buf);
  } catch (err) {
    console.error(`[snapshot] ${parsed.href}:`, err.message);
    return res.status(500).json({ error: 'snapshot error' });
  }
});

// ── Camera proxy (Shodan / manual_ip_seed / insecam_scrape) ────────────────
// Fetches the upstream image server-side so iOS clients can avoid needing an
// ATS exception per arbitrary IP. Strictly bounded: only previously-discovered
// camera_uids are reachable (eliminates SSRF), 5 s timeout, 5 MB cap, image
// content-type required. When the direct fetch fails and the camera's page
// URL is in SNAPSHOT_ALLOWLIST, falls back to the puppeteer snapshot pipeline
// so MJPEG-only and Referer-gated cameras still render.
const PROXY_TTL_MS = 30 * 1000;
const PROXY_MAX_BYTES = 5 * 1024 * 1024;
const PROXY_TIMEOUT_MS = 5000;
const PROXY_CACHE = new Map(); // camera_uid → { bytes, contentType, ts }

router.get('/cameras/proxy', async (req, res) => {
  const uid = String(req.query.camera_uid || '');
  if (!uid) {
    return res.status(400).json({ error: 'camera_uid query param required' });
  }

  const cached = PROXY_CACHE.get(uid);
  if (cached && Date.now() - cached.ts < PROXY_TTL_MS) {
    res.setHeader('Content-Type', cached.contentType);
    res.setHeader('Cache-Control', 'public, max-age=30');
    return res.send(cached.bytes);
  }

  const cam = getCameraByUid(uid);
  if (!cam) return res.status(404).json({ error: 'camera not found' });
  // Prefer the camera's image URL (insecam, etc.) over its page URL (which
  // for aggregators is HTML and can't be served as an image).
  const upstreamUrl = cam.thumbnail_url || cam.url;
  if (!upstreamUrl) return res.status(400).json({ error: 'camera has no url' });

  let parsed;
  try { parsed = new URL(upstreamUrl); }
  catch { return res.status(400).json({ error: 'invalid camera url' }); }
  if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
    return res.status(400).json({ error: 'only http/https allowed' });
  }

  // Snapshot fallback: puppeteer-render `cam.url` when the direct fetch
  // can't produce an image. Generic predicate (host in SNAPSHOT_ALLOWLIST)
  // so any future channel that opts into the snapshot pipeline gets this
  // fallback for free. Returns null when not applicable / capture failed.
  const tryCaptureFallback = async () => {
    const pageUrl = cam.url;
    if (!pageUrl) return null;
    let host;
    try { host = new URL(pageUrl).hostname.toLowerCase(); }
    catch { return null; }
    if (!SNAPSHOT_ALLOWLIST.has(host)) return null;
    const file = snapshotPath(pageUrl);
    const fresh = await readFreshCache(file);
    if (fresh) return fresh;
    try {
      return await captureAndCache(pageUrl, file);
    } catch (err) {
      console.warn(`[cameras/proxy] snapshot fallback for ${uid} failed: ${err.message}`);
      return null;
    }
  };
  const sendBufOrFail = async (errStatus, errBody) => {
    const buf = await tryCaptureFallback();
    if (buf) {
      PROXY_CACHE.set(uid, { bytes: buf, contentType: 'image/jpeg', ts: Date.now() });
      res.setHeader('Content-Type', 'image/jpeg');
      res.setHeader('Cache-Control', 'public, max-age=30');
      return res.send(buf);
    }
    return res.status(errStatus).json(errBody);
  };

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), PROXY_TIMEOUT_MS);
  try {
    const upstream = await fetch(parsed.href, {
      signal: controller.signal,
      headers: {
        'Accept': 'image/*',
        'User-Agent': 'JapanOsintApp/1.0 (camera-proxy)',
      },
      redirect: 'follow',
    });
    if (!upstream.ok) {
      return await sendBufOrFail(502, { error: `upstream ${upstream.status}` });
    }
    const ct = (upstream.headers.get('content-type') || '').split(';')[0].trim();
    if (!ct.startsWith('image/')) {
      return await sendBufOrFail(502, { error: 'upstream did not return an image' });
    }

    // Size-bounded read so a misconfigured camera streaming MJPEG forever
    // doesn't pin our memory. We bail at PROXY_MAX_BYTES; AsyncImage will
    // accept the partial JPEG up to that point or display the failure view.
    const reader = upstream.body.getReader();
    const chunks = [];
    let total = 0;
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      total += value.byteLength;
      if (total > PROXY_MAX_BYTES) {
        try { await reader.cancel(); } catch {}
        return await sendBufOrFail(502, { error: 'upstream too large' });
      }
      chunks.push(Buffer.from(value));
    }
    const bytes = Buffer.concat(chunks);
    PROXY_CACHE.set(uid, { bytes, contentType: ct, ts: Date.now() });
    res.setHeader('Content-Type', ct);
    res.setHeader('Cache-Control', 'public, max-age=30');
    return res.send(bytes);
  } catch (err) {
    if (err.name === 'AbortError') {
      return await sendBufOrFail(504, { error: 'upstream timeout' });
    }
    console.warn(`[cameras/proxy] ${uid} (${parsed.href}) failed: ${err.message}`);
    return await sendBufOrFail(502, { error: 'fetch failed' });
  } finally {
    clearTimeout(timer);
  }
});

// GET /api/data/dam/:damId/live — on-demand scrape of MLIT 水文・水質DB
// for one dam. Triggered from the map popup (see client DamLiveLevel).
// No schedule, no poll — only fires on user click. In-process 5 min cache.
router.get('/dam/:damId/live', async (req, res) => {
  const damId = String(req.params.damId || '').replace(/[^A-Z0-9_]/g, '');
  if (!damId) return res.status(400).json({ ok: false, reason: 'bad damId' });
  try {
    const result = await fetchDamLive(damId);
    res.json(result);
  } catch (err) {
    console.error('[data/dam/live]', err);
    res.status(502).json({ ok: false, reason: err?.message || 'upstream failed' });
  }
});

// GET /api/data/population
router.get('/population', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'estat-population',
    layerType: 'population',
    collectorKey: 'estat-population',
  });
});

// GET /api/data/landprice
router.get('/landprice', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-landprice',
    layerType: 'landprice',
    collectorKey: 'mlit-landprice',
  });
});

// GET /api/data/river
router.get('/river', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-river',
    layerType: 'river',
    collectorKey: 'mlit-river',
  });
});

// GET /api/data/crime
router.get('/crime', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'police-incidents',
    layerType: 'crime',
    collectorKey: 'police-crime',
  });
});

// ===========================================================================
// Social media expansions
// ===========================================================================

router.get('/twitter-geo', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'twitter-geo',
    layerType: 'twitter-geo',
    collectorKey: 'twitter-geo',
  });
});

router.get('/facebook-geo', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'facebook-geo',
    layerType: 'facebook-geo',
    collectorKey: 'facebook-geo',
  });
});

// ===========================================================================
// Marketplace / classifieds
// ===========================================================================

router.get('/classifieds', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'classifieds',
    layerType: 'classifieds',
    collectorKey: 'classifieds',
  });
});

router.get('/real-estate', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'real-estate',
    layerType: 'real-estate',
    collectorKey: 'real-estate',
  });
});

router.get('/job-boards', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'job-boards',
    layerType: 'job-boards',
    collectorKey: 'job-boards',
  });
});

// ===========================================================================
// Cyber OSINT
// ===========================================================================

router.get('/shodan-iot', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'shodan-iot',
    layerType: 'shodan-iot',
    collectorKey: 'shodan-iot',
  });
});

router.get('/wifi-networks', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'wifi-networks',
    layerType: 'wifi-networks',
    collectorKey: 'wifi-networks',
  });
});

// ===========================================================================
// Transport (nationwide)
// ===========================================================================

router.get('/maritime-ais', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'maritime-ais',
    layerType: 'maritime-ais',
    collectorKey: 'maritime-ais',
  });
});

router.get('/flight-adsb', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'flight-adsb',
    layerType: 'flight-adsb',
    collectorKey: 'flight-adsb',
  });
});

router.get('/mlit-n02-stations', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-n02-stations',
    layerType: 'mlit-n02-stations',
    collectorKey: 'mlit-n02-stations',
  });
});

router.get('/bus-routes', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'bus-routes',
    layerType: 'bus-routes',
    collectorKey: 'bus-routes',
  });
});

router.get('/ferry-routes', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'ferry-routes',
    layerType: 'ferry-routes',
    collectorKey: 'ferry-routes',
  });
});

router.get('/highway-traffic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'highway-traffic',
    layerType: 'highway-traffic',
    collectorKey: 'highway-traffic',
  });
});

// ===========================================================================
// Infrastructure
// ===========================================================================

router.get('/electrical-grid', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'electrical-grid',
    layerType: 'electrical-grid',
    collectorKey: 'electrical-grid',
  });
});

router.get('/gas-network', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'gas-network',
    layerType: 'gas-network',
    collectorKey: 'gas-network',
  });
});

router.get('/water-infra', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'water-infra',
    layerType: 'water-infra',
    collectorKey: 'water-infra',
  });
});

router.get('/cell-towers', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'cell-towers',
    layerType: 'cell-towers',
    collectorKey: 'cell-towers',
  });
});

router.get('/nuclear-facilities', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nuclear-facilities',
    layerType: 'nuclear-facilities',
    collectorKey: 'nuclear-facilities',
  });
});

router.get('/ev-charging', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ev-charging', layerType: 'evCharging', collectorKey: 'ev-charging' });
});

router.get('/airport-infra', async (_req, res) => {
  await respondWithData(res, { sourceId: 'airport-infra', layerType: 'airportInfra', collectorKey: 'airport-infra' });
});

router.get('/port-infra', async (_req, res) => {
  await respondWithData(res, { sourceId: 'port-infra', layerType: 'portInfra', collectorKey: 'port-infra' });
});

router.get('/bridge-tunnel-infra', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bridge-tunnel-infra', layerType: 'bridgeTunnelInfra', collectorKey: 'bridge-tunnel-infra' });
});

router.get('/famous-places', async (_req, res) => {
  await respondWithData(res, { sourceId: 'famous-places', layerType: 'famousPlaces', collectorKey: 'famous-places' });
});

// ===========================================================================
// Wave 1: Public Safety + Disaster
// ===========================================================================

router.get('/hospital-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hospital-map',
    layerType: 'hospital-map',
    collectorKey: 'hospital-map',
  });
});

router.get('/aed-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'aed-map',
    layerType: 'aed-map',
    collectorKey: 'aed-map',
  });
});

router.get('/koban-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'koban-map',
    layerType: 'koban-map',
    collectorKey: 'koban-map',
  });
});

router.get('/fire-station-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'fire-station-map',
    layerType: 'fire-station-map',
    collectorKey: 'fire-station-map',
  });
});

router.get('/bosai-shelter', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'bosai-shelter',
    layerType: 'bosai-shelter',
    collectorKey: 'bosai-shelter',
  });
});

router.get('/hazard-map-portal', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hazard-map-portal',
    layerType: 'hazard-map-portal',
    collectorKey: 'hazard-map-portal',
  });
});

router.get('/jshis-seismic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jshis-seismic',
    layerType: 'jshis-seismic',
    collectorKey: 'jshis-seismic',
  });
});

router.get('/hi-net', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hi-net',
    layerType: 'hi-net',
    collectorKey: 'hi-net',
  });
});

router.get('/k-net', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'k-net',
    layerType: 'k-net',
    collectorKey: 'k-net',
  });
});

router.get('/jma-intensity', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-intensity',
    layerType: 'jma-intensity',
    collectorKey: 'jma-intensity',
  });
});

// ===========================================================================
// Wave 2: Health + Statistics + Commerce
// ===========================================================================

router.get('/pharmacy-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'pharmacy-map',
    layerType: 'pharmacy-map',
    collectorKey: 'pharmacy-map',
  });
});

router.get('/convenience-stores', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'convenience-stores',
    layerType: 'convenience-stores',
    collectorKey: 'convenience-stores',
  });
});

router.get('/gas-stations', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'gas-stations',
    layerType: 'gas-stations',
    collectorKey: 'gas-stations',
  });
});

router.get('/tabelog-restaurants', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'tabelog-restaurants',
    layerType: 'tabelog-restaurants',
    collectorKey: 'tabelog-restaurants',
  });
});

router.get('/resas-tourism', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'resas-tourism',
    layerType: 'resas-tourism',
    collectorKey: 'resas-tourism',
  });
});

router.get('/resas-industry', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'resas-industry',
    layerType: 'resas-industry',
    collectorKey: 'resas-industry',
  });
});

router.get('/mlit-transaction', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-transaction',
    layerType: 'mlit-transaction',
    collectorKey: 'mlit-transaction',
  });
});

router.get('/dam-water-level', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'dam-water-level',
    layerType: 'dam-water-level',
    collectorKey: 'dam-water-level',
  });
});

// ===========================================================================
// Wave 3: Maritime + Ocean + Aviation
// ===========================================================================

router.get('/jma-ocean-wave', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-ocean-wave',
    layerType: 'jma-ocean-wave',
    collectorKey: 'jma-ocean-wave',
  });
});

router.get('/jma-ocean-temp', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-ocean-temp',
    layerType: 'jma-ocean-temp',
    collectorKey: 'jma-ocean-temp',
  });
});

router.get('/jma-tide', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-tide',
    layerType: 'jma-tide',
    collectorKey: 'jma-tide',
  });
});

router.get('/nowphas-wave', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nowphas-wave',
    layerType: 'nowphas-wave',
    collectorKey: 'nowphas-wave',
  });
});

router.get('/lighthouse-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'lighthouse-map',
    layerType: 'lighthouse-map',
    collectorKey: 'lighthouse-map',
  });
});

router.get('/jartic-traffic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jartic-traffic',
    layerType: 'jartic-traffic',
    collectorKey: 'jartic-traffic',
  });
});

router.get('/drone-nofly', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'drone-nofly',
    layerType: 'drone-nofly',
    collectorKey: 'drone-nofly',
  });
});

router.get('/jcg-patrol', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jcg-patrol',
    layerType: 'jcg-patrol',
    collectorKey: 'jcg-patrol',
  });
});

// ===========================================================================
// Wave 4: Government + Defense
// ===========================================================================
router.get('/government-buildings', async (_req, res) => {
  await respondWithData(res, { sourceId: 'government-buildings', layerType: 'government-buildings', collectorKey: 'government-buildings' });
});

router.get('/city-halls', async (_req, res) => {
  await respondWithData(res, { sourceId: 'city-halls', layerType: 'city-halls', collectorKey: 'city-halls' });
});

router.get('/courts-prisons', async (_req, res) => {
  await respondWithData(res, { sourceId: 'courts-prisons', layerType: 'courts-prisons', collectorKey: 'courts-prisons' });
});

router.get('/embassies', async (_req, res) => {
  await respondWithData(res, { sourceId: 'embassies', layerType: 'embassies', collectorKey: 'embassies' });
});

router.get('/jsdf-bases', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jsdf-bases', layerType: 'jsdf-bases', collectorKey: 'jsdf-bases' });
});

router.get('/usfj-bases', async (_req, res) => {
  await respondWithData(res, { sourceId: 'usfj-bases', layerType: 'usfj-bases', collectorKey: 'usfj-bases' });
});

router.get('/radar-sites', async (_req, res) => {
  await respondWithData(res, { sourceId: 'radar-sites', layerType: 'radar-sites', collectorKey: 'radar-sites' });
});

router.get('/coast-guard-stations', async (_req, res) => {
  await respondWithData(res, { sourceId: 'coast-guard-stations', layerType: 'coast-guard-stations', collectorKey: 'coast-guard-stations' });
});

// ===========================================================================
// Wave 5: Industry + Energy Deep
// ===========================================================================
router.get('/auto-plants', async (_req, res) => {
  await respondWithData(res, { sourceId: 'auto-plants', layerType: 'auto-plants', collectorKey: 'auto-plants' });
});

router.get('/steel-mills', async (_req, res) => {
  await respondWithData(res, { sourceId: 'steel-mills', layerType: 'steel-mills', collectorKey: 'steel-mills' });
});

router.get('/petrochemical', async (_req, res) => {
  await respondWithData(res, { sourceId: 'petrochemical', layerType: 'petrochemical', collectorKey: 'petrochemical' });
});

router.get('/refineries', async (_req, res) => {
  await respondWithData(res, { sourceId: 'refineries', layerType: 'refineries', collectorKey: 'refineries' });
});

router.get('/semiconductor-fabs', async (_req, res) => {
  await respondWithData(res, { sourceId: 'semiconductor-fabs', layerType: 'semiconductor-fabs', collectorKey: 'semiconductor-fabs' });
});

router.get('/shipyards', async (_req, res) => {
  await respondWithData(res, { sourceId: 'shipyards', layerType: 'shipyards', collectorKey: 'shipyards' });
});

router.get('/petroleum-stockpile', async (_req, res) => {
  await respondWithData(res, { sourceId: 'petroleum-stockpile', layerType: 'petroleum-stockpile', collectorKey: 'petroleum-stockpile' });
});

router.get('/ccs-projects', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ccs-projects', layerType: 'ccs-projects', collectorKey: 'ccs-projects' });
});

router.get('/geothermal-springs', async (_req, res) => {
  await respondWithData(res, { sourceId: 'geothermal-springs', layerType: 'geothermal-springs', collectorKey: 'geothermal-springs' });
});

router.get('/geothermal-projects', async (_req, res) => {
  await respondWithData(res, { sourceId: 'geothermal-projects', layerType: 'geothermal-projects', collectorKey: 'geothermal-projects' });
});

router.get('/wind-turbines', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wind-turbines', layerType: 'wind-turbines', collectorKey: 'wind-turbines' });
});

// ===========================================================================
// Wave 6: Telecom + Internet Infrastructure
// ===========================================================================
router.get('/data-centers', async (_req, res) => {
  await respondWithData(res, { sourceId: 'data-centers', layerType: 'data-centers', collectorKey: 'data-centers' });
});

router.get('/internet-exchanges', async (_req, res) => {
  await respondWithData(res, { sourceId: 'internet-exchanges', layerType: 'internet-exchanges', collectorKey: 'internet-exchanges' });
});

router.get('/submarine-cables', async (_req, res) => {
  await respondWithData(res, { sourceId: 'submarine-cables', layerType: 'submarine-cables', collectorKey: 'submarine-cables' });
});

router.get('/tor-exit-nodes', async (_req, res) => {
  await respondWithData(res, { sourceId: 'tor-exit-nodes', layerType: 'tor-exit-nodes', collectorKey: 'tor-exit-nodes' });
});

router.get('/5g-coverage', async (_req, res) => {
  await respondWithData(res, { sourceId: '5g-coverage', layerType: '5g-coverage', collectorKey: '5g-coverage' });
});

router.get('/satellite-ground-stations', async (_req, res) => {
  await respondWithData(res, { sourceId: 'satellite-ground-stations', layerType: 'satellite-ground-stations', collectorKey: 'satellite-ground-stations' });
});

router.get('/satellite-imagery', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'satellite-imagery',
    layerType: 'satellite-imagery',
    collectorKey: 'satellite-imagery',
  });
});

router.get('/satellite-tracking', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'satellite-tracking',
    layerType: 'satellite-tracking',
    collectorKey: 'satellite-tracking',
  });
});

router.get('/amateur-radio-repeaters', async (_req, res) => {
  await respondWithData(res, { sourceId: 'amateur-radio-repeaters', layerType: 'amateur-radio-repeaters', collectorKey: 'amateur-radio-repeaters' });
});

// ===========================================================================
// Wave 7: Tourism + Culture
// ===========================================================================
router.get('/national-parks', async (_req, res) => {
  await respondWithData(res, { sourceId: 'national-parks', layerType: 'national-parks', collectorKey: 'national-parks' });
});

router.get('/unesco-heritage', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unesco-heritage', layerType: 'unesco-heritage', collectorKey: 'unesco-heritage' });
});

router.get('/castles', async (_req, res) => {
  await respondWithData(res, { sourceId: 'castles', layerType: 'castles', collectorKey: 'castles' });
});

router.get('/museums', async (_req, res) => {
  await respondWithData(res, { sourceId: 'museums', layerType: 'museums', collectorKey: 'museums' });
});

router.get('/stadiums', async (_req, res) => {
  await respondWithData(res, { sourceId: 'stadiums', layerType: 'stadiums', collectorKey: 'stadiums' });
});

router.get('/racetracks', async (_req, res) => {
  await respondWithData(res, { sourceId: 'racetracks', layerType: 'racetracks', collectorKey: 'racetracks' });
});

router.get('/shrine-temple', async (_req, res) => {
  await respondWithData(res, { sourceId: 'shrine-temple', layerType: 'shrine-temple', collectorKey: 'shrine-temple' });
});

router.get('/onsen-map', async (_req, res) => {
  await respondWithData(res, { sourceId: 'onsen-map', layerType: 'onsen-map', collectorKey: 'onsen-map' });
});

router.get('/ski-resorts', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ski-resorts', layerType: 'ski-resorts', collectorKey: 'ski-resorts' });
});

router.get('/anime-pilgrimage', async (_req, res) => {
  await respondWithData(res, { sourceId: 'anime-pilgrimage', layerType: 'anime-pilgrimage', collectorKey: 'anime-pilgrimage' });
});

// ===========================================================================
// Wave 8: Crime + Vice + Wildlife
// ===========================================================================
router.get('/red-light-zones', async (_req, res) => {
  await respondWithData(res, { sourceId: 'red-light-zones', layerType: 'red-light-zones', collectorKey: 'red-light-zones' });
});

router.get('/pachinko-density', async (_req, res) => {
  await respondWithData(res, { sourceId: 'pachinko-density', layerType: 'pachinko-density', collectorKey: 'pachinko-density' });
});

router.get('/wanted-persons', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wanted-persons', layerType: 'wanted-persons', collectorKey: 'wanted-persons' });
});

router.get('/phone-scam-hotspots', async (_req, res) => {
  await respondWithData(res, { sourceId: 'phone-scam-hotspots', layerType: 'phone-scam-hotspots', collectorKey: 'phone-scam-hotspots' });
});

router.get('/pref-police-crime', async (_req, res) => {
  await respondWithData(res, { sourceId: 'pref-police-crime', layerType: 'pref-police-crime', collectorKey: 'pref-police-crime' });
});

router.get('/npa-missing-persons', async (_req, res) => {
  await respondWithData(res, { sourceId: 'npa-missing-persons', layerType: 'npa-missing-persons', collectorKey: 'npa-missing-persons' });
});

router.get('/npa-traffic-accidents', async (_req, res) => {
  await respondWithData(res, { sourceId: 'npa-traffic-accidents', layerType: 'npa-traffic-accidents', collectorKey: 'npa-traffic-accidents' });
});

router.get('/npa-important-wanted', async (_req, res) => {
  await respondWithData(res, { sourceId: 'npa-important-wanted', layerType: 'npa-important-wanted', collectorKey: 'npa-important-wanted' });
});

router.get('/npa-special-fraud', async (_req, res) => {
  await respondWithData(res, { sourceId: 'npa-special-fraud', layerType: 'npa-special-fraud', collectorKey: 'npa-special-fraud' });
});

router.get('/npa-cyber-threat-obs', async (_req, res) => {
  await respondWithData(res, { sourceId: 'npa-cyber-threat-obs', layerType: 'npa-cyber-threat-obs', collectorKey: 'npa-cyber-threat-obs' });
});

router.get('/estat-crime', async (_req, res) => {
  await respondWithData(res, { sourceId: 'estat-crime', layerType: 'estat-crime', collectorKey: 'estat-crime' });
});

router.get('/moj-crime-whitepaper', async (_req, res) => {
  await respondWithData(res, { sourceId: 'moj-crime-whitepaper', layerType: 'moj-crime-whitepaper', collectorKey: 'moj-crime-whitepaper' });
});

// ===========================================================================
// Wave 9: Food + Agriculture
// ===========================================================================
router.get('/sake-breweries', async (_req, res) => {
  await respondWithData(res, { sourceId: 'sake-breweries', layerType: 'sake-breweries', collectorKey: 'sake-breweries' });
});

router.get('/wineries-craftbeer', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wineries-craftbeer', layerType: 'wineries-craftbeer', collectorKey: 'wineries-craftbeer' });
});

router.get('/fish-markets', async (_req, res) => {
  await respondWithData(res, { sourceId: 'fish-markets', layerType: 'fish-markets', collectorKey: 'fish-markets' });
});

router.get('/wagyu-ranches', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wagyu-ranches', layerType: 'wagyu-ranches', collectorKey: 'wagyu-ranches' });
});

router.get('/tea-zones', async (_req, res) => {
  await respondWithData(res, { sourceId: 'tea-zones', layerType: 'tea-zones', collectorKey: 'tea-zones' });
});

router.get('/rice-paddies', async (_req, res) => {
  await respondWithData(res, { sourceId: 'rice-paddies', layerType: 'rice-paddies', collectorKey: 'rice-paddies' });
});

// ===========================================================================
// Wave 10: Niche + Pop Culture
// ===========================================================================
router.get('/vending-machines', async (_req, res) => {
  await respondWithData(res, { sourceId: 'vending-machines', layerType: 'vending-machines', collectorKey: 'vending-machines' });
});

router.get('/karaoke-chains', async (_req, res) => {
  await respondWithData(res, { sourceId: 'karaoke-chains', layerType: 'karaoke-chains', collectorKey: 'karaoke-chains' });
});

router.get('/manga-net-cafes', async (_req, res) => {
  await respondWithData(res, { sourceId: 'manga-net-cafes', layerType: 'manga-net-cafes', collectorKey: 'manga-net-cafes' });
});

router.get('/sento-public-baths', async (_req, res) => {
  await respondWithData(res, { sourceId: 'sento-public-baths', layerType: 'sento-public-baths', collectorKey: 'sento-public-baths' });
});

router.get('/themed-cafes', async (_req, res) => {
  await respondWithData(res, { sourceId: 'themed-cafes', layerType: 'themed-cafes', collectorKey: 'themed-cafes' });
});

// ===========================================================================
// Wave 11: External Mapping Platforms (MarineTraffic, VesselFinder, Sentinel Hub, My Maps)
// ===========================================================================
router.get('/marine-traffic', async (_req, res) => {
  await respondWithData(res, { sourceId: 'marine-traffic', layerType: 'marine-traffic', collectorKey: 'marine-traffic' });
});

router.get('/vessel-finder', async (_req, res) => {
  await respondWithData(res, { sourceId: 'vessel-finder', layerType: 'vessel-finder', collectorKey: 'vessel-finder' });
});

router.get('/google-my-maps', async (_req, res) => {
  await respondWithData(res, { sourceId: 'google-my-maps', layerType: 'google-my-maps', collectorKey: 'google-my-maps' });
});

// ===========================================================================
// Wave 12: Untapped OSM infrastructure tags
// ===========================================================================
router.get('/parking-facilities', async (_req, res) => {
  await respondWithData(res, { sourceId: 'parking-facilities', layerType: 'parking-facilities', collectorKey: 'parking-facilities' });
});

router.get('/water-towers', async (_req, res) => {
  await respondWithData(res, { sourceId: 'water-towers', layerType: 'water-towers', collectorKey: 'water-towers' });
});

router.get('/transmission-towers', async (_req, res) => {
  await respondWithData(res, { sourceId: 'transmission-towers', layerType: 'transmission-towers', collectorKey: 'transmission-towers' });
});

router.get('/utility-poles', async (_req, res) => {
  await respondWithData(res, { sourceId: 'utility-poles', layerType: 'utility-poles', collectorKey: 'utility-poles' });
});

router.get('/admin-boundaries', async (_req, res) => {
  await respondWithData(res, { sourceId: 'admin-boundaries', layerType: 'admin-boundaries', collectorKey: 'admin-boundaries' });
});

// ===========================================================================
// Wave 12: Unified Camera Discovery
// ===========================================================================
router.get('/camera-discovery', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'camera-discovery',
    layerType: 'camera-discovery',
    collectorKey: 'camera-discovery',
  });
});

// ===========================================================================
// Wave 13: Net-new live OSINT endpoints (2026 sweep)
// ===========================================================================
router.get('/p2pquake', async (_req, res) => {
  await respondWithData(res, { sourceId: 'p2pquake-jma', layerType: 'earthquake', collectorKey: 'p2pquake-jma' });
});

router.get('/wolfx-eew', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wolfx-eew', layerType: 'earthquake', collectorKey: 'wolfx-eew' });
});

router.get('/wolfx-eqlist', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wolfx-eqlist', layerType: 'earthquake', collectorKey: 'wolfx-eqlist' });
});

router.get('/jma-forecast-area', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jma-forecast-area', layerType: 'weather', collectorKey: 'jma-forecast-area' });
});

router.get('/jma-typhoon-json', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jma-typhoon-json', layerType: 'weather', collectorKey: 'jma-typhoon-json' });
});

router.get('/openmeteo-jma', async (_req, res) => {
  await respondWithData(res, { sourceId: 'openmeteo-jma', layerType: 'weather', collectorKey: 'openmeteo-jma' });
});

router.get('/nerv-feed', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nerv-feed', layerType: 'earthquake', collectorKey: 'nerv-feed' });
});

router.get('/msil-umishiru', async (_req, res) => {
  await respondWithData(res, { sourceId: 'msil-umishiru', layerType: 'maritime-ais', collectorKey: 'msil-umishiru' });
});

router.get('/jcg-navarea', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jcg-navarea', layerType: 'maritime-ais', collectorKey: 'jcg-navarea' });
});

router.get('/edinet-filings', async (_req, res) => {
  await respondWithData(res, { sourceId: 'edinet-filings', layerType: 'edinet-filings', collectorKey: 'edinet-filings' });
});

router.get('/boj-stats', async (_req, res) => {
  await respondWithData(res, { sourceId: 'boj-stats', layerType: 'edinet-filings', collectorKey: 'boj-stats' });
});

router.get('/egov-laws', async (_req, res) => {
  await respondWithData(res, { sourceId: 'egov-laws', layerType: 'edinet-filings', collectorKey: 'egov-laws' });
});

router.get('/data-go-jp-ckan', async (_req, res) => {
  await respondWithData(res, { sourceId: 'data-go-jp-ckan', layerType: 'edinet-filings', collectorKey: 'data-go-jp-ckan' });
});

router.get('/geospatial-jp-ckan', async (_req, res) => {
  await respondWithData(res, { sourceId: 'geospatial-jp-ckan', layerType: 'edinet-filings', collectorKey: 'geospatial-jp-ckan' });
});

router.get('/nhk-news-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nhk-news-rss', layerType: 'news-feed', collectorKey: 'nhk-news-rss' });
});

router.get('/nhk-world-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nhk-world-rss', layerType: 'news-feed', collectorKey: 'nhk-world-rss' });
});

router.get('/kyodo-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'kyodo-rss', layerType: 'news-feed', collectorKey: 'kyodo-rss' });
});

router.get('/wifi-hotspots-jcfw', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wifi-hotspots-jcfw', layerType: 'wifi-hotspots', collectorKey: 'wifi-hotspots-jcfw' });
});

router.get('/wifi-hotspots-freespot', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wifi-hotspots-freespot', layerType: 'wifi-hotspots', collectorKey: 'wifi-hotspots-freespot' });
});

router.get('/jpcert-alerts-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jpcert-alerts-rss', layerType: 'jpcert-alerts', collectorKey: 'jpcert-alerts-rss' });
});

router.get('/nict-atlas', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nict-atlas', layerType: 'nicter-darknet', collectorKey: 'nict-atlas' });
});

router.get('/gsi-geocode', async (_req, res) => {
  await respondWithData(res, { sourceId: 'gsi-geocode', layerType: 'geocode', collectorKey: 'gsi-geocode' });
});

router.get('/japan-api-prefectures', async (_req, res) => {
  await respondWithData(res, { sourceId: 'japan-api-prefectures', layerType: 'admin-boundaries', collectorKey: 'japan-api-prefectures' });
});

// Wave 11: broadened pulse collectors
router.get('/hatena-bookmark', async (_req, res) => {
  await respondWithData(res, { sourceId: 'hatena-bookmark', layerType: 'hatena-bookmark', collectorKey: 'hatena-bookmark' });
});
router.get('/certstream-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'certstream-jp', layerType: 'certstream-jp', collectorKey: 'certstream-jp' });
});
router.get('/japan-post-offices', async (_req, res) => {
  await respondWithData(res, { sourceId: 'japan-post-offices', layerType: 'japan-post-offices', collectorKey: 'japan-post-offices' });
});

// Wave 11 (cont.)
router.get('/wdcgg-co2', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wdcgg-co2', layerType: 'wdcgg-co2', collectorKey: 'wdcgg-co2' });
});
router.get('/censys-japan', async (_req, res) => {
  await respondWithData(res, { sourceId: 'censys-japan', layerType: 'censys-japan', collectorKey: 'censys-japan' });
});
router.get('/nicter-stats', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nicter-stats', layerType: 'nicter-stats', collectorKey: 'nicter-stats' });
});
router.get('/misskey-timeline', async (_req, res) => {
  await respondWithData(res, { sourceId: 'misskey-timeline', layerType: 'misskey-timeline', collectorKey: 'misskey-timeline' });
});
router.get('/bird-makeup-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bird-makeup-jp', layerType: 'bird-makeup-jp', collectorKey: 'bird-makeup-jp' });
});
router.get('/suumo-rental-density', async (_req, res) => {
  await respondWithData(res, { sourceId: 'suumo-rental-density', layerType: 'suumo-rental-density', collectorKey: 'suumo-rental-density' });
});
router.get('/note-com-trending', async (_req, res) => {
  await respondWithData(res, { sourceId: 'note-com-trending', layerType: 'note-com-trending', collectorKey: 'note-com-trending' });
});
router.get('/mercari-trending', async (_req, res) => {
  await respondWithData(res, { sourceId: 'mercari-trending', layerType: 'mercari-trending', collectorKey: 'mercari-trending' });
});
router.get('/greynoise-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'greynoise-jp', layerType: 'greynoise-jp', collectorKey: 'greynoise-jp' });
});

// Wave 14a: Fused expressway layer
router.get('/unified-highway', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unified-highway', layerType: 'unified-highway', collectorKey: 'unified-highway' });
});

// Wave 14: Offensive-recon OSINT
router.get('/fofa-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'fofa-jp', layerType: 'fofa-jp', collectorKey: 'fofa-jp' });
});
router.get('/quake360-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'quake360-jp', layerType: 'quake360-jp', collectorKey: 'quake360-jp' });
});
router.get('/urlscan-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'urlscan-jp', layerType: 'urlscan-jp', collectorKey: 'urlscan-jp' });
});
router.get('/wayback-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wayback-jp', layerType: 'wayback-jp', collectorKey: 'wayback-jp' });
});
router.get('/github-leaks-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'github-leaks-jp', layerType: 'github-leaks-jp', collectorKey: 'github-leaks-jp' });
});
router.get('/chan-5ch', async (_req, res) => {
  await respondWithData(res, { sourceId: 'chan-5ch', layerType: 'chan-5ch', collectorKey: 'chan-5ch' });
});
router.get('/houjin-bangou', async (_req, res) => {
  await respondWithData(res, { sourceId: 'houjin-bangou', layerType: 'houjin-bangou', collectorKey: 'houjin-bangou' });
});
router.get('/strava-heatmap-bases', async (_req, res) => {
  await respondWithData(res, { sourceId: 'strava-heatmap-bases', layerType: 'strava-heatmap-bases', collectorKey: 'strava-heatmap-bases' });
});
router.get('/ipa-alerts-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ipa-alerts-rss', layerType: 'ipa-alerts', collectorKey: 'ipa-alerts-rss' });
});
router.get('/grayhat-buckets', async (_req, res) => {
  await respondWithData(res, { sourceId: 'grayhat-buckets', layerType: 'grayhat-buckets', collectorKey: 'grayhat-buckets' });
});

// ── Wave 15: high-penetrance vuln/threat/breach + SOCINT additions ──────
// Vuln intel
router.get('/my-jvn', async (_req, res) => {
  await respondWithData(res, { sourceId: 'my-jvn', layerType: 'my-jvn', collectorKey: 'my-jvn' });
});
router.get('/cisa-kev-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'cisa-kev-jp', layerType: 'cisa-kev-jp', collectorKey: 'cisa-kev-jp' });
});
router.get('/osv-dev', async (_req, res) => {
  await respondWithData(res, { sourceId: 'osv-dev', layerType: 'osv-dev', collectorKey: 'osv-dev' });
});
router.get('/ghsa-advisories', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ghsa-advisories', layerType: 'ghsa-advisories', collectorKey: 'ghsa-advisories' });
});
router.get('/poc-in-github', async (_req, res) => {
  await respondWithData(res, { sourceId: 'poc-in-github', layerType: 'poc-in-github', collectorKey: 'poc-in-github' });
});
router.get('/trickest-cve', async (_req, res) => {
  await respondWithData(res, { sourceId: 'trickest-cve', layerType: 'trickest-cve', collectorKey: 'trickest-cve' });
});

// IOC / attacker activity
router.get('/shadowserver-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'shadowserver-jp', layerType: 'shadowserver-jp', collectorKey: 'shadowserver-jp' });
});
router.get('/urlhaus-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'urlhaus-jp', layerType: 'urlhaus-jp', collectorKey: 'urlhaus-jp' });
});
router.get('/threatfox-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'threatfox-jp', layerType: 'threatfox-jp', collectorKey: 'threatfox-jp' });
});
router.get('/feodo-tracker-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'feodo-tracker-jp', layerType: 'feodo-tracker-jp', collectorKey: 'feodo-tracker-jp' });
});
router.get('/sslbl-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'sslbl-jp', layerType: 'sslbl-jp', collectorKey: 'sslbl-jp' });
});
router.get('/spamhaus-drop', async (_req, res) => {
  await respondWithData(res, { sourceId: 'spamhaus-drop', layerType: 'spamhaus-drop', collectorKey: 'spamhaus-drop' });
});
router.get('/abuseipdb-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'abuseipdb-jp', layerType: 'abuseipdb-jp', collectorKey: 'abuseipdb-jp' });
});
router.get('/alienvault-otx-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'alienvault-otx-jp', layerType: 'alienvault-otx-jp', collectorKey: 'alienvault-otx-jp' });
});
router.get('/phishing-feeds-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'phishing-feeds-jp', layerType: 'phishing-feeds-jp', collectorKey: 'phishing-feeds-jp' });
});
router.get('/sans-isc', async (_req, res) => {
  await respondWithData(res, { sourceId: 'sans-isc', layerType: 'sans-isc', collectorKey: 'sans-isc' });
});

// Asset / breach intel
router.get('/leakix-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'leakix-jp', layerType: 'leakix-jp', collectorKey: 'leakix-jp' });
});
router.get('/netlas-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'netlas-jp', layerType: 'netlas-jp', collectorKey: 'netlas-jp' });
});
router.get('/hudson-rock-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'hudson-rock-jp', layerType: 'hudson-rock-jp', collectorKey: 'hudson-rock-jp' });
});
router.get('/virustotal-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'virustotal-jp', layerType: 'virustotal-jp', collectorKey: 'virustotal-jp' });
});
router.get('/chaos-bugbounty-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'chaos-bugbounty-jp', layerType: 'chaos-bugbounty-jp', collectorKey: 'chaos-bugbounty-jp' });
});

// Network / BGP / DNS history
router.get('/peeringdb-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'peeringdb-jp', layerType: 'peeringdb-jp', collectorKey: 'peeringdb-jp' });
});
router.get('/bgp-tools-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bgp-tools-jp', layerType: 'bgp-tools-jp', collectorKey: 'bgp-tools-jp' });
});
router.get('/crtsh-historical', async (_req, res) => {
  await respondWithData(res, { sourceId: 'crtsh-historical', layerType: 'crtsh-historical', collectorKey: 'crtsh-historical' });
});
router.get('/cloudflare-radar-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'cloudflare-radar-jp', layerType: 'cloudflare-radar-jp', collectorKey: 'cloudflare-radar-jp' });
});
router.get('/ooni-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ooni-jp', layerType: 'ooni-jp', collectorKey: 'ooni-jp' });
});
router.get('/ioda-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ioda-jp', layerType: 'ioda-jp', collectorKey: 'ioda-jp' });
});
router.get('/ripestat-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ripestat-jp', layerType: 'ripestat-jp', collectorKey: 'ripestat-jp' });
});

// SOCINT / news
router.get('/yahoo-realtime', async (_req, res) => {
  await respondWithData(res, { sourceId: 'yahoo-realtime', layerType: 'yahoo-realtime', collectorKey: 'yahoo-realtime' });
});
router.get('/mastodon-jp-instances', async (_req, res) => {
  await respondWithData(res, { sourceId: 'mastodon-jp-instances', layerType: 'mastodon-jp-instances', collectorKey: 'mastodon-jp-instances' });
});
router.get('/bluesky-jetstream-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bluesky-jetstream-jp', layerType: 'bluesky-jetstream-jp', collectorKey: 'bluesky-jetstream-jp' });
});
router.get('/niconico-ranking', async (_req, res) => {
  await respondWithData(res, { sourceId: 'niconico-ranking', layerType: 'niconico-ranking', collectorKey: 'niconico-ranking' });
});
router.get('/wikipedia-ja-recent', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wikipedia-ja-recent', layerType: 'wikipedia-ja-recent', collectorKey: 'wikipedia-ja-recent' });
});
router.get('/osm-changesets-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'osm-changesets-jp', layerType: 'osm-changesets-jp', collectorKey: 'osm-changesets-jp' });
});
router.get('/yahoo-news-jp-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'yahoo-news-jp-rss', layerType: 'yahoo-news-jp-rss', collectorKey: 'yahoo-news-jp-rss' });
});
router.get('/jp-news-rss', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jp-news-rss', layerType: 'jp-news-rss', collectorKey: 'jp-news-rss' });
});

// Geo / disaster
router.get('/nasa-firms-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'nasa-firms-jp', layerType: 'nasa-firms-jp', collectorKey: 'nasa-firms-jp' });
});

export default router;
