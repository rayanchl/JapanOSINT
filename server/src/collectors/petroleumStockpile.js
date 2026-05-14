/**
 * Petroleum Stockpile Collector
 *
 * 10 national oil + 5 national LPG reserve bases (operated by JOGMEC) plus
 * a hand-curated set of major commercial oil distribution depots. The 15
 * national bases are enriched per request from JOGMEC's reserve-base /
 * storage-base detail pages — capacity, area, completion year, address —
 * with hardcoded coordinates as the geocoding source of truth (the JOGMEC
 * pages publish addresses, not lat/lon).
 *
 * Overpass/OSM is also queried and appended, so storage tanks tagged in OSM
 * but outside the 15 named bases still surface (commercial racks, etc).
 */

import { fetchOverpass, fetchText } from './_liveHelpers.js';

// 15 national bases (10 oil + 5 LPG). slug = JOGMEC URL slug under
// /about/domestic-offices/{reserve-base|storage-base}/{slug}.html
const NATIONAL_BASES = [
  // Oil — /reserve-base/{slug}.html
  { slug: 'tomakomai',     name: '苫小牧東部国家石油備蓄基地', lat: 42.6044, lon: 141.6919, kind: 'national',     capacity_kl: 6400000 },
  { slug: 'mutsu-ogawara', name: 'むつ小川原国家石油備蓄基地', lat: 40.9744, lon: 141.3814, kind: 'national',     capacity_kl: 5700000 },
  { slug: 'kuji',          name: '久慈国家石油備蓄基地',       lat: 40.2292, lon: 141.7956, kind: 'national',     capacity_kl: 1750000 },
  { slug: 'akita',         name: '秋田国家石油備蓄基地',       lat: 39.7706, lon: 140.0561, kind: 'national',     capacity_kl: 4500000 },
  { slug: 'shibushi',      name: '志布志国家石油備蓄基地',     lat: 31.4878, lon: 131.0997, kind: 'national',     capacity_kl: 5000000 },
  { slug: 'kamigoto',      name: '上五島国家石油備蓄基地',     lat: 32.9981, lon: 129.0850, kind: 'national',     capacity_kl: 4400000 },
  { slug: 'shiratori',     name: '白島国家石油備蓄基地',       lat: 33.9389, lon: 130.7194, kind: 'national',     capacity_kl: 5600000 },
  { slug: 'fukui',         name: '福井国家石油備蓄基地',       lat: 35.7800, lon: 136.0717, kind: 'national',     capacity_kl: 3400000 },
  { slug: 'kikuma',        name: '菊間国家石油備蓄基地',       lat: 34.0461, lon: 132.8983, kind: 'national',     capacity_kl: 1500000 },
  { slug: 'kushikino',     name: '串木野国家石油備蓄基地',     lat: 31.6864, lon: 130.2697, kind: 'national',     capacity_kl: 1750000 },
  // LPG — /storage-base/{slug}.html
  { slug: 'nanao',         name: '七尾国家石油ガス備蓄基地',           lat: 37.0567, lon: 136.9569, kind: 'national_lpg', capacity_t: 250000 },
  { slug: 'fukushima',     name: '福島国家石油ガス備蓄基地 (相馬)',    lat: 37.7967, lon: 140.9694, kind: 'national_lpg', capacity_t: 200000 },
  { slug: 'namikata',      name: '波方国家石油ガス備蓄基地',           lat: 34.1356, lon: 132.9572, kind: 'national_lpg', capacity_t: 450000 },
  { slug: 'kurashiki',     name: '倉敷国家石油ガス備蓄基地',           lat: 34.5036, lon: 133.7669, kind: 'national_lpg', capacity_t: 400000 },
  { slug: 'kamisu',        name: '神栖国家石油ガス備蓄基地',           lat: 35.9133, lon: 140.6906, kind: 'national_lpg', capacity_t: 200000 },
];

const COMMERCIAL_DEPOTS = [
  { name: 'ENEOS 川崎油槽所',           lat: 35.5189, lon: 139.7372, capacity_kl: 800000 },
  { name: '出光 千葉油槽所',             lat: 35.4781, lon: 140.0719, capacity_kl: 600000 },
  { name: 'コスモ 千葉油槽所',           lat: 35.4658, lon: 140.1011, capacity_kl: 750000 },
  { name: 'ENEOS 仙台油槽所',           lat: 38.2244, lon: 141.0319, capacity_kl: 400000 },
  { name: 'ENEOS 根岸油槽所',           lat: 35.4014, lon: 139.6311, capacity_kl: 1200000 },
  { name: 'ENEOS 水島油槽所',           lat: 34.4961, lon: 133.7344, capacity_kl: 900000 },
  { name: 'ENEOS 大分油槽所',           lat: 33.2700, lon: 131.7283, capacity_kl: 500000 },
  { name: '出光 北海道油槽所 (苫小牧)', lat: 42.5828, lon: 141.5839, capacity_kl: 700000 },
];

const JOGMEC_BASE = 'https://www.jogmec.go.jp/about/domestic-offices';

function stripTags(s) {
  return s.replace(/<[^>]+>/g, ' ').replace(/&nbsp;/g, ' ').replace(/\s+/g, ' ').trim();
}

// "約640" → 640.  Returns null if no number found.
function parseManNumber(s) {
  const m = s.match(/([\d,]+(?:\.\d+)?)/);
  if (!m) return null;
  const n = parseFloat(m[1].replace(/,/g, ''));
  return Number.isFinite(n) ? n : null;
}

/**
 * Pull capacity, area, address, completion year from a JOGMEC base detail page.
 * Returns null on fetch / parse miss — never throws.
 */
async function scrapeJogmecBase(slug, kind) {
  const path = kind === 'national_lpg' ? 'storage-base' : 'reserve-base';
  const url = `${JOGMEC_BASE}/${path}/${slug}.html`;
  const html = await fetchText(url, { timeoutMs: 10_000, retries: 1 });
  if (!html) return null;

  // Extract <th>label</th><td>value</td> rows from the page's first dataTable.
  const rowRe = /<tr\b[^>]*>\s*<th[^>]*>([\s\S]*?)<\/th>\s*<td[^>]*>([\s\S]*?)<\/td>\s*<\/tr>/g;
  const rows = {};
  let m;
  while ((m = rowRe.exec(html)) !== null) {
    rows[stripTags(m[1])] = stripTags(m[2]);
  }
  if (Object.keys(rows).length === 0) return null;

  // Field name varies between oil (所在地及び面積 — combined cell) and LPG
  // (separate 所在地 + 面積 cells).
  const locationCell = rows['所在地及び面積'] ?? rows['所在地'] ?? null;
  const areaCell     = rows['所在地及び面積'] ?? rows['面積']   ?? null;
  const capacityCell = rows['備蓄施設容量']   ?? rows['施設容量'] ?? null;
  const completionCell = rows['完成年等']     ?? rows['完成時期'] ?? null;
  const methodCell     = rows['備蓄方式']     ?? rows['方式']     ?? null;

  // Address: strip the trailing "（約NNNヘクタール）" if combined-form.
  let address = null;
  if (locationCell) {
    address = locationCell.replace(/[（(]約?[\d,.]+\s*ヘクタール[）)]/g, '').trim();
  }

  // Area in ha
  let area_ha = null;
  if (areaCell) {
    const am = areaCell.match(/約?([\d,]+(?:\.\d+)?)\s*ヘクタール/);
    if (am) area_ha = parseFloat(am[1].replace(/,/g, ''));
  }

  // Capacity. Oil pages: "約640万キロリットル". LPG pages: "約25万トン".
  let capacity_kl = null;
  let capacity_t  = null;
  if (capacityCell) {
    const klMatch = capacityCell.match(/約?([\d,]+(?:\.\d+)?)\s*万\s*キロリットル/);
    if (klMatch) capacity_kl = parseFloat(klMatch[1].replace(/,/g, '')) * 10_000;
    const tMatch = capacityCell.match(/約?([\d,]+(?:\.\d+)?)\s*万\s*トン/);
    if (tMatch) capacity_t = parseFloat(tMatch[1].replace(/,/g, '')) * 10_000;
  }

  // Completion year — first 4-digit year mentioned in the cell.
  let completed_year = null;
  if (completionCell) {
    const ym = completionCell.match(/(\d{4})\s*年/);
    if (ym) completed_year = parseInt(ym[1], 10);
  }

  return {
    address,
    area_ha,
    capacity_kl,
    capacity_t,
    completed_year,
    storage_method: methodCell || null,
    source_url: url,
  };
}

async function tryOverpass() {
  return await fetchOverpass(
    'node["man_made"="storage_tank"]["content"="oil"](area.jp);way["man_made"="storage_tank"]["content"="oil"](area.jp);node["industrial"="oil"](area.jp);way["industrial"="oil"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        reserve_id: `STOCK_LIVE_${String(i + 1).padStart(5, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Oil storage ${el.id}`,
        kind: el.tags?.industrial === 'oil' ? 'commercial' : 'storage_tank',
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'petroleum_stockpile_osm',
      },
    })
  );
}

export default async function collectPetroleumStockpile() {
  const scraped = await Promise.all(
    NATIONAL_BASES.map((b) => scrapeJogmecBase(b.slug, b.kind))
  );

  const features = [];
  let liveCount = 0;

  NATIONAL_BASES.forEach((b, i) => {
    const live = scraped[i];
    if (live) liveCount++;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
      properties: {
        reserve_id: `STOCK_${String(i + 1).padStart(5, '0')}`,
        name: b.name,
        kind: b.kind,
        capacity_kl:    live?.capacity_kl ?? b.capacity_kl ?? null,
        capacity_t:     live?.capacity_t  ?? b.capacity_t  ?? null,
        area_ha:        live?.area_ha        ?? null,
        completed_year: live?.completed_year ?? null,
        storage_method: live?.storage_method ?? null,
        address:        live?.address        ?? null,
        country: 'JP',
        operator: 'JOGMEC',
        source: live ? 'petroleum_stockpile_jogmec' : 'petroleum_stockpile_seed_fallback',
        source_url: live?.source_url ?? `${JOGMEC_BASE}/${b.kind === 'national_lpg' ? 'storage-base' : 'reserve-base'}/${b.slug}.html`,
      },
    });
  });

  COMMERCIAL_DEPOTS.forEach((d, i) => {
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
      properties: {
        reserve_id: `STOCK_C_${String(i + 1).padStart(5, '0')}`,
        name: d.name,
        kind: 'commercial',
        capacity_kl: d.capacity_kl,
        country: 'JP',
        source: 'petroleum_stockpile_seed',
      },
    });
  });

  // Append OSM-tagged storage tanks that aren't part of the 15 named bases.
  const osm = await tryOverpass();
  if (Array.isArray(osm)) features.push(...osm);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'petroleum_stockpile',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: liveCount > 0,
      scraped_national_count: liveCount,
      description: 'JOGMEC strategic petroleum reserves (10 oil + 5 LPG, live-scraped) + 8 major commercial depots + OSM-tagged storage tanks',
    },
  };
}
