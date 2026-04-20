import express from 'express';
import { getLinesByMode } from '../utils/transportStore.js';

const router = express.Router();

const VALID_MODES = new Set(['train', 'subway', 'bus']);
// Upper bound on features per response. Hit mainly by `mode=train`, where
// fused OSM fragments can produce 100k+ rows; cutting off at 5k keeps the
// response a few megabytes at most while still covering any single
// metropolitan area in detail.
const MAX_FEATURES = 5000;

function parseBbox(raw) {
  if (!raw) return null;
  const parts = String(raw).split(',').map(Number);
  if (parts.length !== 4 || parts.some((n) => !Number.isFinite(n))) return 'invalid';
  const [minLng, minLat, maxLng, maxLat] = parts;
  return { minLng, minLat, maxLng, maxLat };
}

function lineIntersectsBbox(coords, bbox) {
  for (const [lng, lat] of coords) {
    if (lng >= bbox.minLng && lng <= bbox.maxLng && lat >= bbox.minLat && lat <= bbox.maxLat) {
      return true;
    }
  }
  return false;
}

// Compact route catalogue consumed by the client-side live-vehicle simulator
// (useLiveVehicles). Only geometry + identity + color are forwarded; richer
// data lives on the unified_* layer endpoints. Optional `bbox=minLng,minLat,
// maxLng,maxLat` clips the result to a viewport; without it, responses are
// capped at MAX_FEATURES and a `_meta.truncated` flag is set.
router.get('/routes', (req, res) => {
  const mode = String(req.query.mode || '').toLowerCase();
  if (!VALID_MODES.has(mode)) {
    return res.status(400).json({ error: 'mode must be train|subway|bus' });
  }
  const bbox = parseBbox(req.query.bbox);
  if (bbox === 'invalid') {
    return res.status(400).json({ error: 'bbox must be minLng,minLat,maxLng,maxLat' });
  }
  try {
    const lines = getLinesByMode(mode);
    const features = [];
    let truncated = false;
    for (const line of lines) {
      const coords = line?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
      if (bbox && !lineIntersectsBbox(coords, bbox)) continue;
      if (features.length >= MAX_FEATURES) { truncated = true; break; }
      const p = line.properties || {};
      features.push({
        type: 'Feature',
        geometry: { type: 'LineString', coordinates: coords },
        properties: {
          route_id: p.line_uid || null,
          name: p.name || null,
          line_color: p.line_color || null,
        },
      });
    }
    const body = { type: 'FeatureCollection', features };
    if (truncated) body._meta = { truncated: true, limit: MAX_FEATURES };
    res.json(body);
  } catch (err) {
    console.error('[transit/routes]', err);
    res.status(500).json({ error: 'internal' });
  }
});

import { ingestFeedZip } from '../utils/gtfsIngest.js';
import {
  isOperatorHydrated,
  markOperatorHydrated,
  getDeparturesAt,
  listHydratedOperators,
} from '../utils/gtfsStore.js';

const GTFS_API = 'https://api.gtfs-data.jp/v2';

// One-flight guard: concurrent hydrate calls for the same orgId share the
// same in-flight promise so we don't download/ingest the same feeds twice.
const inflightHydrate = new Map();

async function hydrateOperator(orgId) {
  if (isOperatorHydrated(orgId)) return { cached: true };
  const existing = inflightHydrate.get(orgId);
  if (existing) return existing;

  const p = (async () => {
    const feedsRes = await fetch(`${GTFS_API}/organizations/${orgId}/feeds`);
    if (!feedsRes.ok) throw new Error(`feed list HTTP ${feedsRes.status}`);
    const body = await feedsRes.json();
    const feeds = Array.isArray(body?.body) ? body.body : [];
    const feedIds = [];
    const totals = { routes: 0, trips: 0, stop_times: 0, shapes: 0, calendar: 0, stops: 0 };
    for (const f of feeds) {
      const feedId = f.feed_id || f.id;
      if (!feedId) continue;
      const zipRes = await fetch(
        `${GTFS_API}/organizations/${orgId}/feeds/${feedId}/files/archive.zip`,
      );
      if (!zipRes.ok) continue;
      const buf = await zipRes.arrayBuffer();
      try {
        const c = ingestFeedZip(orgId, feedId, buf);
        for (const k of Object.keys(totals)) {
          if (typeof c[k] === 'number') totals[k] += c[k];
        }
        feedIds.push(feedId);
      } catch (err) {
        console.error(`[transit/hydrate] ${orgId}/${feedId} ingest failed:`, err?.message);
      }
    }
    markOperatorHydrated(orgId, orgId, feedIds, {
      stops: totals.stops,
      trips: totals.trips,
    });
    return { cached: false, feedIds, counts: totals };
  })();

  inflightHydrate.set(orgId, p);
  try {
    return await p;
  } finally {
    inflightHydrate.delete(orgId);
  }
}

router.post('/gtfs/hydrate/:orgId', async (req, res) => {
  const raw = String(req.params.orgId || '');
  const orgId = raw.replace(/[^a-z0-9_-]/gi, '');
  if (!orgId) return res.status(400).json({ error: 'bad orgId' });
  try {
    const result = await hydrateOperator(orgId);
    res.json({ ok: true, orgId, ...result });
  } catch (err) {
    console.error('[transit/gtfs/hydrate]', err);
    res.status(502).json({ error: 'hydrate failed', detail: err?.message });
  }
});

router.get('/gtfs/stop/:stopId/departures', (req, res) => {
  const stopId = String(req.params.stopId || '');
  if (!stopId) return res.status(400).json({ error: 'missing stopId' });
  const limit = Math.min(20, Math.max(1, Number(req.query.limit) || 5));
  const t = req.query.t ? new Date(String(req.query.t)) : new Date();
  if (isNaN(t.getTime())) return res.status(400).json({ error: 'bad t (ISO date)' });
  try {
    const departures = getDeparturesAt(stopId, t, limit);
    res.json({ stop_id: stopId, now: t.toISOString(), departures });
  } catch (err) {
    console.error('[transit/gtfs/departures]', err);
    res.status(500).json({ error: 'internal' });
  }
});

router.get('/gtfs/operators', (_req, res) => {
  try {
    res.json({ operators: listHydratedOperators() });
  } catch (err) {
    console.error('[transit/gtfs/operators]', err);
    res.status(500).json({ error: 'internal' });
  }
});

export default router;

