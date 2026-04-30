import { Router } from 'express';
import crypto from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { getSourceById } from '../utils/database.js';
import { collectors } from '../collectors/index.js';
import { captureSnapshot } from '../utils/screenshot.js';
import { isRunInFlight, runCameraDiscovery } from '../utils/cameraRunner.js';
import { withCollectorRun, annotateLastHit, getBroadcaster } from '../utils/collectorTap.js';
import { getOAuthToken } from '../utils/openskyAuth.js';
import { getEnrich, setEnrich } from '../utils/flightEnrichCache.js';
import { fetchDamLive } from '../utils/mlitDamLive.js';
import {
  getCached, setCached, getTtlMs,
} from '../utils/collectorCache.js';
import {
  broadcastLayerWorkStarted, broadcastLayerWorkFinished,
} from '../utils/layerEvents.js';

const router = Router();

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
 * Helper: run the collector live for this source, or return an empty
 * FeatureCollection with status info if no collector is registered.
 *
 * Caching: on every call we check collector_cache first (TTL per source
 * from collector_ttls). On hit the cached FC returns immediately; on miss
 * we invoke the collector, normalise, write to cache, and broadcast
 * layer_work_* WS events so the client spinner fires on server work.
 */
async function respondWithData(res, { sourceId, layerType, collectorKey }) {
  try {
    const collector = collectorKey ? collectors[collectorKey] : null;
    if (!collector) {
      const source = getSourceById(sourceId);
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

    // 1. Cache hit short-circuit
    const cached = getCached(collectorKey);
    if (cached) {
      const fc = cached.fc;
      // Re-stamp cache telemetry onto the response so the client can show
      // freshness. _meta is preserved; only cache_status / age_ms overwrite.
      fc._meta = {
        ...(fc._meta || {}),
        cache_status: 'hit',
        age_ms: cached.ageMs,
        ttl_ms: getTtlMs(collectorKey),
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

    const fc = normaliseFc(data, collectorKey);
    const recordCount = fc.features.length;
    annotateLastHit({ record_count: recordCount });

    // Persist for the source's TTL window
    const ttlMs = getTtlMs(collectorKey);
    setCached(collectorKey, fc, ttlMs);

    // Decorate the outgoing response with cache status
    fc._meta = { ...fc._meta, cache_status: 'miss', age_ms: 0, ttl_ms: ttlMs };

    broadcastLayerWorkFinished({
      layerId: layerType,
      collectorKey,
      sourceId,
      durationMs: Date.now() - startedAt,
      recordCount,
      cacheStatus: 'miss',
    });

    return res.json(fc);
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

// GET /api/data/social
router.get('/social', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'twitter-geo',
    layerType: 'social',
    collectorKey: 'social-media',
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

router.get('/insecam-webcams', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'insecam-webcams',
    layerType: 'insecam-webcams',
    collectorKey: 'insecam-webcams',
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

router.get('/full-transport', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'full-transport',
    layerType: 'full-transport',
    collectorKey: 'full-transport',
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
    layerType: 'satelliteImagery',
    collectorKey: 'satellite-imagery',
  });
});

router.get('/satellite-tracking', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'satellite-tracking',
    layerType: 'satelliteTracking',
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
router.get('/yakuza-hq', async (_req, res) => {
  await respondWithData(res, { sourceId: 'yakuza-hq', layerType: 'yakuza-hq', collectorKey: 'yakuza-hq' });
});

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
router.get('/kanagawa-police', async (_req, res) => {
  await respondWithData(res, { sourceId: 'kanagawa-police', layerType: 'kanagawa-police', collectorKey: 'kanagawa-police' });
});
router.get('/greynoise-jp', async (_req, res) => {
  await respondWithData(res, { sourceId: 'greynoise-jp', layerType: 'greynoise-jp', collectorKey: 'greynoise-jp' });
});

export default router;
