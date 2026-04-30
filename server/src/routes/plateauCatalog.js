/**
 * PLATEAU 3D Tiles catalog proxy.
 *
 * Calls the official MLIT Project PLATEAU GraphQL catalog
 * (https://api.plateau.reearth.io/datacatalog/graphql) once per 24h and
 * returns the nationwide list of `bldg` (building) Cesium 3D Tiles
 * tilesets so the client can mount them as a deck.gl Tile3DLayer overlay.
 *
 * The upstream catalog covers 300+ Japanese cities with per-LOD tilesets;
 * the client doesn't need to talk GraphQL itself, and we don't burn the
 * upstream on every page load.
 */

import { Router } from 'express';

const router = Router();

const GRAPHQL_URL = 'https://api.plateau.reearth.io/datacatalog/graphql';
const USER_AGENT = 'JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)';
const CACHE_TTL_MS = 24 * 60 * 60 * 1000;

const CATALOG_QUERY = `
  query NationwideBuildingTilesets {
    datasets(input: { includeTypes: ["bldg"] }) {
      __typename
      ... on PlateauDataset {
        id
        name
        year
        prefectureCode
        cityCode
        prefecture { name }
        city { name }
        items {
          id
          format
          url
          lod
          texture
        }
      }
    }
  }
`;

let cache = null; // { fetchedAt, tilesets }

async function fetchCatalog() {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 20_000);
  try {
    const res = await fetch(GRAPHQL_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': USER_AGENT,
        Accept: 'application/json',
      },
      body: JSON.stringify({ query: CATALOG_QUERY }),
      signal: ctrl.signal,
    });
    if (!res.ok) throw new Error(`PLATEAU catalog HTTP ${res.status}`);
    const json = await res.json();
    if (json.errors?.length) throw new Error(`PLATEAU GraphQL: ${json.errors[0].message}`);
    return json.data?.datasets ?? [];
  } finally {
    clearTimeout(timer);
  }
}

// Per dataset, pick exactly one tileset item: prefer LOD1 (full coverage,
// reasonable mesh weight). Some cities only ship LOD2 (Tokyo wards) — fall
// back to that. Skip LOD3/4 (textured high-detail) by default; the client
// can request a richer view later via ?lod=2.
function selectItem(items, preferredLod) {
  if (!items?.length) return null;
  const byLod = (lod) => items.find((i) => i.format === 'CESIUM3DTILES' && i.lod === lod);
  return byLod(preferredLod) || byLod(1) || byLod(2) || byLod(0)
    || items.find((i) => i.format === 'CESIUM3DTILES') || null;
}

function shapeTilesets(datasets, preferredLod) {
  const out = [];
  for (const ds of datasets) {
    const item = selectItem(ds.items, preferredLod);
    if (!item || !item.url) continue;
    out.push({
      id: ds.id,
      city: ds.city?.name ?? null,
      prefecture: ds.prefecture?.name ?? null,
      cityCode: ds.cityCode ?? null,
      prefectureCode: ds.prefectureCode ?? null,
      year: ds.year ?? null,
      lod: item.lod ?? null,
      tilesetUrl: item.url,
    });
  }
  return out;
}

async function getCatalog() {
  if (cache && Date.now() - cache.fetchedAt < CACHE_TTL_MS) return cache.datasets;
  const datasets = await fetchCatalog();
  cache = { fetchedAt: Date.now(), datasets };
  return datasets;
}

// GET /api/plateau/tilesets[?lod=1|2]
router.get('/tilesets', async (req, res) => {
  const lodParam = parseInt(req.query.lod, 10);
  const preferredLod = Number.isFinite(lodParam) ? lodParam : 1;
  try {
    const datasets = await getCatalog();
    const tilesets = shapeTilesets(datasets, preferredLod);
    res.set('Cache-Control', 'public, max-age=3600');
    res.json({
      tilesets,
      count: tilesets.length,
      preferredLod,
      fetchedAt: cache.fetchedAt,
    });
  } catch (err) {
    console.warn('[plateau/tilesets] catalog fetch failed:', err?.message);
    res.status(502).json({ error: 'catalog_unavailable', message: err?.message });
  }
});

export default router;
