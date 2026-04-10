/**
 * Bird Flu Outbreaks Collector
 * MAFF HPAI (Highly Pathogenic Avian Influenza) confirmed farm outbreaks.
 * Live: MAFF Agriculture portal JSON / RSS.
 */

import { fetchJson, fetchText } from './_liveHelpers.js';

const MAFF_JSON = 'https://www.maff.go.jp/j/syouan/douei/tori/attach/outbreaks.json';
const MAFF_RSS = 'https://www.maff.go.jp/j/syouan/douei/tori/rss.xml';

async function tryMaffJson() {
  const data = await fetchJson(MAFF_JSON, { timeoutMs: 8000 });
  if (!data || !Array.isArray(data?.outbreaks)) return null;
  return data.outbreaks.slice(0, 200).map((o, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [o.lon, o.lat] },
    properties: {
      outbreak_id: `MAFF_${i + 1}`,
      date_confirmed: o.date,
      prefecture: o.prefecture,
      city: o.city,
      strain: o.strain || 'H5N1',
      species: o.species || 'chicken',
      birds_culled: o.culled || null,
      country: 'JP',
      source: 'maff_hpai',
    },
  }));
}

async function tryMaffRss() {
  const xml = await fetchText(MAFF_RSS, { timeoutMs: 8000 });
  if (!xml) return null;
  const items = xml.match(/<item[\s\S]*?<\/item>/g) || [];
  if (items.length === 0) return null;
  return items.slice(0, 50).map((it, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.6917, 35.6896] },
    properties: {
      outbreak_id: `MAFF_RSS_${i + 1}`,
      title: (it.match(/<title>(.*?)<\/title>/) || [])[1] || null,
      pub_date: (it.match(/<pubDate>(.*?)<\/pubDate>/) || [])[1] || null,
      country: 'JP',
      source: 'maff_hpai_rss',
    },
  }));
}

// Recent major outbreak seasons (2020-2024 seasons) — representative prefecture cities where culling occurred
const SEED_OUTBREAKS = [
  { prefecture: '北海道', lat: 43.0628, lon: 141.3478, strain: 'H5N1', season: '2023-24', birds_culled: 40000, cases: 3 },
  { prefecture: '青森県', lat: 40.8244, lon: 140.7400, strain: 'H5N1', season: '2022-23', birds_culled: 80000, cases: 2 },
  { prefecture: '秋田県', lat: 39.7186, lon: 140.1024, strain: 'H5N1', season: '2022-23', birds_culled: 1460000, cases: 4 },
  { prefecture: '岩手県', lat: 39.7036, lon: 141.1525, strain: 'H5N1', season: '2021-22', birds_culled: 30000, cases: 2 },
  { prefecture: '山形県', lat: 38.2403, lon: 140.3633, strain: 'H5N1', season: '2022-23', birds_culled: 100000, cases: 2 },
  { prefecture: '宮城県', lat: 38.2683, lon: 140.8719, strain: 'H5N1', season: '2022-23', birds_culled: 42000, cases: 1 },
  { prefecture: '福島県', lat: 37.7503, lon: 140.4675, strain: 'H5N1', season: '2020-21', birds_culled: 110000, cases: 1 },
  { prefecture: '茨城県', lat: 36.3658, lon: 140.4711, strain: 'H5N1', season: '2022-23', birds_culled: 1200000, cases: 5 },
  { prefecture: '千葉県', lat: 35.6083, lon: 140.1233, strain: 'H5N1', season: '2022-23', birds_culled: 3300000, cases: 8 },
  { prefecture: '埼玉県', lat: 35.8617, lon: 139.6455, strain: 'H5N1', season: '2022-23', birds_culled: 110000, cases: 1 },
  { prefecture: '群馬県', lat: 36.3911, lon: 139.0608, strain: 'H5N1', season: '2022-23', birds_culled: 60000, cases: 1 },
  { prefecture: '新潟県', lat: 37.9161, lon: 139.0364, strain: 'H5N1', season: '2021-22', birds_culled: 380000, cases: 4 },
  { prefecture: '長野県', lat: 36.6489, lon: 138.1944, strain: 'H5N1', season: '2022-23', birds_culled: 200000, cases: 2 },
  { prefecture: '岐阜県', lat: 35.4233, lon: 136.7606, strain: 'H5N1', season: '2020-21', birds_culled: 1200000, cases: 7 },
  { prefecture: '愛知県', lat: 35.1814, lon: 136.9069, strain: 'H5N1', season: '2022-23', birds_culled: 340000, cases: 3 },
  { prefecture: '三重県', lat: 34.7184, lon: 136.5067, strain: 'H5N1', season: '2022-23', birds_culled: 290000, cases: 2 },
  { prefecture: '滋賀県', lat: 35.0044, lon: 135.8686, strain: 'H5N1', season: '2020-21', birds_culled: 10000, cases: 1 },
  { prefecture: '京都府', lat: 35.0116, lon: 135.7681, strain: 'H5N1', season: '2021-22', birds_culled: 140000, cases: 1 },
  { prefecture: '大阪府', lat: 34.6864, lon: 135.5197, strain: 'H5N1', season: '2022-23', birds_culled: 120000, cases: 1 },
  { prefecture: '兵庫県', lat: 34.6913, lon: 135.1830, strain: 'H5N1', season: '2022-23', birds_culled: 190000, cases: 2 },
  { prefecture: '奈良県', lat: 34.6850, lon: 135.8048, strain: 'H5N1', season: '2022-23', birds_culled: 98000, cases: 1 },
  { prefecture: '岡山県', lat: 34.6628, lon: 133.9197, strain: 'H5N1', season: '2021-22', birds_culled: 250000, cases: 2 },
  { prefecture: '広島県', lat: 34.3853, lon: 132.4553, strain: 'H5N1', season: '2020-21', birds_culled: 170000, cases: 2 },
  { prefecture: '山口県', lat: 34.1856, lon: 131.4714, strain: 'H5N1', season: '2022-23', birds_culled: 75000, cases: 1 },
  { prefecture: '香川県', lat: 34.3401, lon: 134.0434, strain: 'H5N1', season: '2020-21', birds_culled: 2600000, cases: 17 },
  { prefecture: '徳島県', lat: 34.0658, lon: 134.5594, strain: 'H5N1', season: '2020-21', birds_culled: 240000, cases: 1 },
  { prefecture: '愛媛県', lat: 33.8392, lon: 132.7656, strain: 'H5N1', season: '2022-23', birds_culled: 320000, cases: 2 },
  { prefecture: '高知県', lat: 33.5594, lon: 133.5311, strain: 'H5N1', season: '2022-23', birds_culled: 57000, cases: 1 },
  { prefecture: '福岡県', lat: 33.5904, lon: 130.4017, strain: 'H5N1', season: '2020-21', birds_culled: 910000, cases: 3 },
  { prefecture: '佐賀県', lat: 33.2494, lon: 130.2989, strain: 'H5N1', season: '2022-23', birds_culled: 26000, cases: 1 },
  { prefecture: '長崎県', lat: 32.7503, lon: 129.8775, strain: 'H5N1', season: '2022-23', birds_culled: 39000, cases: 1 },
  { prefecture: '熊本県', lat: 32.8019, lon: 130.7256, strain: 'H5N1', season: '2022-23', birds_culled: 90000, cases: 1 },
  { prefecture: '大分県', lat: 33.2381, lon: 131.6126, strain: 'H5N1', season: '2021-22', birds_culled: 150000, cases: 1 },
  { prefecture: '宮崎県', lat: 31.9111, lon: 131.4239, strain: 'H5N1', season: '2020-21', birds_culled: 2530000, cases: 13 },
  { prefecture: '鹿児島県', lat: 31.5963, lon: 130.5571, strain: 'H5N1', season: '2022-23', birds_culled: 1040000, cases: 8 },
];

function generateSeedData() {
  return SEED_OUTBREAKS.map((o, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [o.lon, o.lat] },
    properties: {
      outbreak_id: `HPAI_${String(i + 1).padStart(4, '0')}`,
      prefecture: o.prefecture,
      strain: o.strain,
      season: o.season,
      birds_culled: o.birds_culled,
      farm_cases: o.cases,
      country: 'JP',
      source: 'maff_hpai_seed',
    },
  }));
}

export default async function collectBirdFluOutbreaks() {
  let features = await tryMaffJson();
  let liveSource = 'maff_hpai';
  if (!features || features.length === 0) {
    features = await tryMaffRss();
    liveSource = 'maff_hpai_rss';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'maff_hpai_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'bird-flu-outbreaks',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'MAFF HPAI (avian influenza) confirmed farm outbreaks across Japan',
    },
    metadata: {},
  };
}
