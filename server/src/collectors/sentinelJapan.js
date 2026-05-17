/**
 * Sentinel over Japan collector (sentinel-japan) — kind:'intel'.
 *
 * Live: Copernicus Sentinel Hub Catalog STAC API. Authenticates via OAuth2
 * client-credentials using SENTINELHUB_CLIENT_ID / SENTINELHUB_CLIENT_SECRET
 * (read through envFor / BYOK). Queries the most recent Sentinel-2 L2A scenes
 * intersecting the Japan bbox and emits one intel item per scene describing
 * the latest available imagery (raster product — not a point cloud).
 *
 * Honest empty when no credentials (source 'sentinel-japan_no_key') or when
 * the upstream catalog fetch/auth fails ('sentinel-japan_unavailable').
 * Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'sentinel-japan';
// minLon, minLat, maxLon, maxLat
const JP_BBOX = [122.0, 24.0, 146.0, 46.0];
const SCENE_LIMIT = 50;
const CLOUD_MAX = 40;

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
}

async function getAccessToken(clientId, clientSecret) {
  const body = new URLSearchParams({
    grant_type: 'client_credentials',
    client_id: clientId,
    client_secret: clientSecret,
  });
  // Sentinel Hub OAuth token endpoint (Copernicus Data Space production cluster).
  const data = await fetchJson(
    'https://services.sentinel-hub.com/oauth/token',
    {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body,
      timeoutMs: 12000,
    },
  ).catch(() => null);
  return data?.access_token || null;
}

function centroid(geom) {
  try {
    const ring = geom?.type === 'Polygon'
      ? geom.coordinates?.[0]
      : geom?.type === 'MultiPolygon'
        ? geom.coordinates?.[0]?.[0]
        : null;
    if (!ring?.length) return null;
    const s = ring.reduce((a, [x, y]) => [a[0] + x, a[1] + y], [0, 0]);
    return [s[0] / ring.length, s[1] / ring.length];
  } catch { return null; }
}

export default async function collectSentinelJapan() {
  const clientId = envFor('SENTINELHUB_CLIENT_ID');
  const clientSecret = envFor('SENTINELHUB_CLIENT_SECRET');
  if (!clientId || !clientSecret) {
    return empty('sentinel-japan_no_key',
      'Copernicus Sentinel-2 scenes over Japan (requires SENTINELHUB_CLIENT_ID/SECRET)');
  }

  const token = await getAccessToken(clientId, clientSecret);
  if (!token) {
    return empty('sentinel-japan_unavailable',
      'Copernicus Sentinel-2 scenes over Japan — OAuth token request failed');
  }

  const to = new Date();
  const from = new Date(Date.now() - 21 * 86400e3);
  const reqBody = {
    bbox: JP_BBOX,
    datetime: `${from.toISOString()}/${to.toISOString()}`,
    collections: ['sentinel-2-l2a'],
    limit: SCENE_LIMIT,
    filter: { op: '<=', args: [{ property: 'eo:cloud_cover' }, CLOUD_MAX] },
  };
  const data = await fetchJson(
    'https://services.sentinel-hub.com/api/v1/catalog/1.0.0/search',
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${token}`,
      },
      body: JSON.stringify(reqBody),
      timeoutMs: 15000,
    },
  ).catch(() => null);

  const feats = data?.features;
  if (!Array.isArray(feats) || feats.length === 0) {
    return empty('sentinel-japan_unavailable',
      'Copernicus Sentinel-2 scenes over Japan — catalog returned no scenes');
  }

  const items = feats.map((f, i) => {
    const geom = f.geometry || null;
    const c = centroid(geom);
    const acquired = f.properties?.datetime || null;
    const cloud = f.properties?.['eo:cloud_cover'] ?? null;
    const sid = f.id || `scene-${i}`;
    return {
      uid: intelUid(SOURCE_ID, sid),
      title: `Sentinel-2 L2A scene ${sid}`,
      summary: `Sentinel-2 L2A acquired ${acquired || 'unknown'}${cloud != null ? `, ${cloud}% cloud` : ''} over Japan.`,
      body: `Copernicus Sentinel-2 Level-2A optical scene ${sid} intersecting the Japan AOI. Acquired ${acquired || 'unknown'}. Cloud cover ${cloud != null ? `${cloud}%` : 'unknown'}.`,
      link: 'https://dataspace.copernicus.eu/',
      published_at: acquired || new Date().toISOString(),
      tags: ['satellite', 'sentinel-2', 'copernicus', 'optical', 'raster'],
      properties: {
        bbox: f.bbox || JP_BBOX,
        geometry: geom,
        centroid: c,
        acquired,
        cloud_cover: cloud,
        sensor: 'MSI',
        platform: f.properties?.platform || 'sentinel-2',
        scene_id: sid,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'Latest Copernicus Sentinel-2 L2A scenes over Japan (Sentinel Hub Catalog)',
    extraMeta: { source: 'sentinel-japan' },
  });
}
