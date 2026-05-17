/**
 * MLIT National Land Numerical Information — land use mesh (nlni-landuse).
 *
 * MLIT publishes the authoritative nationwide land-use segmented mesh
 * (土地利用細分メッシュ, dataset code L03-b) at:
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-L03-b.html
 * The official distribution is GML/Shapefile (no clean bulk GeoJSON API and
 * the full national mesh is very large), so we accept a pre-converted /
 * subset GeoJSON URL via the optional env var MLIT_L03B_GEOJSON_URL and do a
 * real best-effort fetch of it (plus year mirrors). With no usable source we
 * return an HONEST EMPTY FeatureCollection — no fabricated land-use polygons.
 *
 * NOTE: MLIT does not host a GeoJSON of this dataset directly; the mirrors
 * below are UNVERIFIED best-effort candidates. Empty-on-miss keeps it honest.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelHashKey } from '../utils/intelHelpers.js';

// Land-use classification codes (L03-b 土地利用種別) → human label.
const LANDUSE_LABELS = {
  '0100': '田', '0200': 'その他の農用地', '0500': '森林',
  '0600': '荒地', '0700': '建物用地', '0901': '道路',
  '0902': '鉄道', '1000': 'その他の用地', '1100': '河川地及び湖沼',
  '1400': '海浜', '1500': '海水域', '1600': 'ゴルフ場',
};

// Unverified best-effort mirror candidates (no official GeoJSON endpoint).
const MIRRORS = [];

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'MLIT KSJ L03-b national land-use segmented mesh (best-effort, requires MLIT_L03B_GEOJSON_URL)',
    },
  };
}

function normalize(data) {
  const src = Array.isArray(data?.features) ? data.features : null;
  if (!src) return [];
  const out = [];
  for (const f of src) {
    if (!f?.geometry || !f.geometry.type || f.geometry.coordinates == null) continue;
    const p = f.properties || {};
    const code = String(
      p.landuse ?? p.L03b_002 ?? p.L03b_001 ?? p.code ?? '',
    ).trim();
    out.push({
      type: 'Feature',
      geometry: f.geometry,
      properties: {
        mesh_id:
          p.mesh ?? p.L03b_001 ?? p.id ?? `L03B_${intelHashKey(JSON.stringify(f.geometry))}`,
        landuse_code: code || null,
        landuse: LANDUSE_LABELS[code] ?? p.landuse_name ?? null,
        source: 'mlit_l03b',
      },
    });
  }
  return out;
}

export default async function collectNlniLanduse() {
  const override = envFor('MLIT_L03B_GEOJSON_URL');
  const urls = [override, ...MIRRORS].filter(Boolean);
  if (urls.length === 0) return emptyResult('nlni_landuse_no_source');

  for (const url of urls) {
    let data = null;
    try {
      data = await fetchJson(url, { timeoutMs: 30000 });
    } catch {
      data = null;
    }
    if (!data) continue;
    const features = normalize(data);
    if (features.length > 0) {
      return {
        type: 'FeatureCollection',
        features,
        _meta: {
          source: 'mlit_l03b',
          fetchedAt: new Date().toISOString(),
          recordCount: features.length,
          description: 'MLIT KSJ L03-b national land-use segmented mesh (best-effort, requires MLIT_L03B_GEOJSON_URL)',
        },
      };
    }
  }

  return emptyResult('nlni_landuse_unavailable');
}
