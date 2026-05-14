/**
 * GTFS-JP — Standardized Japanese Bus Schedule Data
 * 標準的なバス情報フォーマット / GTFS Data Repository Japan
 *
 *   https://www.gtfs-data.jp/
 *
 * Aggregates 400+ Japanese bus operator feeds in GTFS-JP format.
 * This collector lists currently-published feeds (organizations, feeds,
 * stops count). For any single feed it can lazy-fetch and expose stops.txt
 * as GeoJSON points.
 *
 * Env:
 *   GTFS_JP_FEED_LIMIT   (default 20)   — max feeds to hydrate stops from
 *   GTFS_JP_STOP_CAP     (default 50000) — hard cap on total stops returned
 *
 * Without network this falls back to OSM `highway=bus_stop` — same as P11,
 * but exposes it under a distinct collector key so the scheduler can run
 * both independently and the fuse layer can dedupe them.
 */

import { fetchJson, fetchText, fetchOverpassTiled } from './_liveHelpers.js';

const API_BASE = 'https://api.gtfs-data.jp/v2';
const FEED_LIMIT = parseInt(process.env.GTFS_JP_FEED_LIMIT || '20', 10);
const STOP_CAP = parseInt(process.env.GTFS_JP_STOP_CAP || '50000', 10);

/** List all organizations (bus operators) on gtfs-data.jp */
async function listOrganizations() {
  try {
    const res = await fetchJson(`${API_BASE}/organizations`, { timeoutMs: 15000 });
    if (!res || !Array.isArray(res.body)) return [];
    return res.body;
  } catch {
    return [];
  }
}

/** List feeds for a single organization */
async function listFeeds(orgId) {
  try {
    const res = await fetchJson(
      `${API_BASE}/organizations/${orgId}/feeds`,
      { timeoutMs: 15000 },
    );
    if (!res || !Array.isArray(res.body)) return [];
    return res.body;
  } catch {
    return [];
  }
}

/**
 * Fetch stops.txt for one published feed and return GeoJSON points.
 * GTFS-JP stops.txt CSV has columns: stop_id,stop_name,stop_lat,stop_lon,...
 */
async function fetchFeedStops(orgId, feedId, source_label) {
  const url = `${API_BASE}/organizations/${orgId}/feeds/${feedId}/files/stops.txt`;
  const text = await fetchText(url, { timeoutMs: 20000 });
  if (!text) return [];

  const lines = text.split(/\r?\n/).filter(Boolean);
  if (lines.length < 2) return [];

  const header = lines[0].split(',').map(s => s.replace(/^"|"$/g, '').trim());
  const ix = (name) => header.indexOf(name);
  const iId = ix('stop_id'), iName = ix('stop_name');
  const iLat = ix('stop_lat'), iLon = ix('stop_lon');
  if (iLat < 0 || iLon < 0) return [];

  const out = [];
  for (let r = 1; r < lines.length; r++) {
    // naive CSV split — GTFS files rarely use quoted commas in stops.txt
    const cols = lines[r].split(',');
    const lat = parseFloat(cols[iLat]);
    const lon = parseFloat(cols[iLon]);
    const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
    out.push({
      type: 'Feature',
      geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
      properties: {
        stop_id: `GTFSJP_${orgId}_${(cols[iId] || r).replace(/"/g, '')}`,
        name: (cols[iName] || '').replace(/^"|"$/g, '') || null,
        operator: source_label || orgId,
        feed_id: feedId,
        source: 'gtfs_jp',
      },
    });
  }
  return out;
}

async function tryLive() {
  const orgs = await listOrganizations();
  if (!orgs.length) return null;

  const allStops = [];
  let feedsProcessed = 0;

  outer:
  for (const org of orgs) {
    const orgId = org.organization_id || org.id || org.organizationID;
    const orgName = org.organization_name || org.name || orgId;
    if (!orgId) continue;

    const feeds = await listFeeds(orgId);
    for (const feed of feeds) {
      const feedId = feed.feed_id || feed.id;
      if (!feedId) continue;
      const stops = await fetchFeedStops(orgId, feedId, orgName);
      if (stops.length) allStops.push(...stops);
      feedsProcessed++;
      if (feedsProcessed >= FEED_LIMIT) break outer;
      if (allStops.length >= STOP_CAP) break outer;
    }
  }

  return allStops.length ? allStops.slice(0, STOP_CAP) : null;
}

async function tryOsmFallback() {
  return fetchOverpassTiled(
    (bbox) => `node["highway"="bus_stop"](${bbox});`,
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        stop_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Bus stop',
        name_ja: el.tags?.name || null,
        operator: el.tags?.operator || el.tags?.network || null,
        route: el.tags?.route_ref || null,
        source: 'osm_overpass_bus_stop_fallback',
      },
    }),
    { queryTimeout: 180, timeoutMs: 120_000 },
  );
}

export default async function collectGtfsJp() {
  let features = null;
  let liveSrc = null;

  try {
    features = await tryLive();
    if (features && features.length) liveSrc = 'gtfs_jp_live';
  } catch {
    features = null;
  }

  if (!features) {
    features = await tryOsmFallback();
    if (features && features.length) liveSrc = 'osm_overpass_bus_stop_fallback';
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'gtfs_jp_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      feed_limit: FEED_LIMIT,
      stop_cap: STOP_CAP,
      description: 'GTFS-JP nationwide bus schedule aggregator (gtfs-data.jp, 400+ operators)',
    },
  };
}
