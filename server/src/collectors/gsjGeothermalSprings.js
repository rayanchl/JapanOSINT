/**
 * GSJ Geothermal / Onsen Spring Chemistry Database — 7,200+ analysis points.
 *
 * Source: https://gbank.gsj.jp/gres-db/  (Geological Survey of Japan, AIST)
 * Released 2020 as a single static KML; refreshes are rare so we let the
 * standard collector cache (24 h ceiling) carry it.
 *
 * Each placemark is a small ~100 m polygon outlining the spring's analysis
 * site, with name + 80-odd chemistry fields embedded in <description> CDATA.
 * We project to a point (first polygon vertex), and surface the small set of
 * fields useful at the layer level: name, prefecture, municipality, depth,
 * temperature, pH, total dissolved solids.
 */

import { fetchText } from './_liveHelpers.js';

const KML_URL = 'https://gbank.gsj.jp/gres-db/download/onsen/GSJ_DB_GRES-DB_ONSEN_2020.kml';

function inJapanBbox(lon, lat) {
  return lon >= 122 && lon <= 154 && lat >= 24 && lat <= 46;
}

function pickRow(desc, label) {
  const re = new RegExp(`${label}\\s*[：:]\\s*([^<\\n]+?)<br`);
  const m = desc.match(re);
  if (!m) return null;
  const raw = m[1].trim();
  if (!raw || raw === 'N/A') return null;
  return raw;
}

function pickNumber(desc, label) {
  const raw = pickRow(desc, label);
  if (raw === null) return null;
  const n = parseFloat(raw);
  return Number.isFinite(n) ? n : null;
}

function parseKml(kml) {
  const features = [];
  const placemarkRe = /<Placemark\b[^>]*>([\s\S]*?)<\/Placemark>/g;
  let m;
  let i = 0;
  while ((m = placemarkRe.exec(kml)) !== null) {
    const block = m[1];
    const coordMatch = block.match(/<coordinates>\s*([\s\S]*?)\s*<\/coordinates>/);
    if (!coordMatch) continue;
    const firstTuple = coordMatch[1].trim().split(/\s+/)[0] || '';
    const [lonStr, latStr] = firstTuple.split(',');
    const lon = parseFloat(lonStr);
    const lat = parseFloat(latStr);
    if (!Number.isFinite(lon) || !Number.isFinite(lat)) continue;
    if (!inJapanBbox(lon, lat)) continue;

    const descMatch = block.match(/<description>\s*<!\[CDATA\[([\s\S]*?)\]\]>\s*<\/description>/);
    const desc = descMatch ? descMatch[1] : '';

    i += 1;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        spring_id: `SPRING_${String(i).padStart(5, '0')}`,
        name_ja: pickRow(desc, '温泉名'),
        source_name_ja: pickRow(desc, '泉源名'),
        prefecture: pickRow(desc, '都道府県'),
        municipality: pickRow(desc, '市区町村'),
        depth_m: pickNumber(desc, '深度'),
        temperature_c: pickNumber(desc, '泉温'),
        flow_l_min: pickNumber(desc, '湧出量'),
        ph: pickNumber(desc, 'pH'),
        tds_mg_kg: pickNumber(desc, 'TSM'),
        sample_date: pickRow(desc, '採水年月日'),
        country: 'JP',
        source: 'gsj_gres_db_onsen_2020',
      },
    });
  }
  return features;
}

export default async function collectGsjGeothermalSprings() {
  const kml = await fetchText(KML_URL, { timeoutMs: 60_000, retries: 1 });
  if (!kml) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'geothermal-springs',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        live: false,
        description: 'GSJ GRES-DB onsen KML fetch failed',
      },
    };
  }

  const features = parseKml(kml);
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'geothermal-springs',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'GSJ Geothermal Resource Database (GRES-DB) — onsen / spring chemistry analysis points (2020 release)',
    },
  };
}
