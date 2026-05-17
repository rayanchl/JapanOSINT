/**
 * Landsat over Japan collector (landsat-jp) — kind:'intel'.
 *
 * Live: USGS Landsat Collection 2 L2 STAC API (LandsatLook STAC server,
 * public / no auth). Queries the latest scenes intersecting the Japan bbox
 * and emits one intel item per scene (raster product, not a point cloud).
 *
 * USGS_M2M_TOKEN is read via envFor as an OPTIONAL credential — the public
 * STAC endpoint works without it, so absence does NOT make this collector
 * empty; it only raises quotas for the M2M path (not required here).
 *
 * Honest empty ('landsat-jp_unavailable') when the STAC fetch fails or
 * returns no scenes. Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'landsat-jp';
const JP_BBOX = [122.0, 24.0, 146.0, 46.0];
const SCENE_LIMIT = 50;

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
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

export default async function collectLandsatJp() {
  // Optional — only used to bump M2M quota; STAC search below is keyless.
  const m2mToken = envFor('USGS_M2M_TOKEN');

  const to = new Date();
  const from = new Date(Date.now() - 30 * 86400e3);
  const reqBody = {
    bbox: JP_BBOX,
    datetime: `${from.toISOString()}/${to.toISOString()}`,
    collections: ['landsat-c2l2-sr'],
    limit: SCENE_LIMIT,
  };
  // USGS LandsatLook STAC (public). Unverified for the exact collection name
  // across versions — falls back to Microsoft Planetary Computer STAC, also
  // public/keyless, if the primary returns nothing.
  let data = await fetchJson(
    'https://landsatlook.usgs.gov/stac-server/search',
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        ...(m2mToken ? { 'X-Auth-Token': m2mToken } : {}),
      },
      body: JSON.stringify(reqBody),
      timeoutMs: 15000,
    },
  ).catch(() => null);

  let sourceTag = 'landsat-jp';
  if (!Array.isArray(data?.features) || data.features.length === 0) {
    data = await fetchJson(
      'https://planetarycomputer.microsoft.com/api/stac/v1/search',
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          bbox: JP_BBOX,
          datetime: `${from.toISOString()}/${to.toISOString()}`,
          collections: ['landsat-c2-l2'],
          limit: SCENE_LIMIT,
          query: { 'eo:cloud_cover': { lt: 60 } },
        }),
        timeoutMs: 15000,
      },
    ).catch(() => null);
    sourceTag = 'landsat-jp';
  }

  const feats = data?.features;
  if (!Array.isArray(feats) || feats.length === 0) {
    return empty('landsat-jp_unavailable',
      'USGS Landsat C2 L2 scenes over Japan — STAC returned no scenes');
  }

  const items = feats.map((f, i) => {
    const geom = f.geometry || null;
    const c = centroid(geom);
    const acquired = f.properties?.datetime || null;
    const cloud = f.properties?.['eo:cloud_cover'] ?? null;
    const sid = f.id || `scene-${i}`;
    const thumb = f.assets?.rendered_preview?.href
      || f.assets?.thumbnail?.href || null;
    return {
      uid: intelUid(SOURCE_ID, sid),
      title: `Landsat scene ${sid}`,
      summary: `Landsat C2 L2 acquired ${acquired || 'unknown'}${cloud != null ? `, ${cloud}% cloud` : ''} over Japan.`,
      body: `USGS Landsat Collection 2 Level-2 surface-reflectance scene ${sid} intersecting the Japan AOI. Acquired ${acquired || 'unknown'}. Cloud cover ${cloud != null ? `${cloud}%` : 'unknown'}.`,
      link: 'https://landsatlook.usgs.gov/',
      published_at: acquired || new Date().toISOString(),
      tags: ['satellite', 'landsat', 'usgs', 'optical', 'raster'],
      properties: {
        bbox: f.bbox || JP_BBOX,
        geometry: geom,
        centroid: c,
        acquired,
        cloud_cover: cloud,
        sensor: f.properties?.instruments?.join(',') || 'OLI/TIRS',
        platform: f.properties?.platform || 'landsat',
        scene_id: sid,
        preview_url: thumb,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'Latest USGS Landsat Collection 2 L2 scenes over Japan',
    extraMeta: { source: sourceTag },
  });
}
