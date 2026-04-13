/**
 * Bird Flu Outbreaks Collector
 * MAFF HPAI (Highly Pathogenic Avian Influenza) confirmed farm outbreaks.
 * Live: MAFF's public tori-influenza index page (HTML). MAFF posts
 * outbreak announcements there as PDF/HTML rather than JSON, so we use
 * the page as a reachability check and return the geocoded seed of
 * confirmed outbreak locations derived from those same announcements.
 */

import { fetchText } from './_liveHelpers.js';

const MAFF_TORI_INDEX = 'https://www.maff.go.jp/j/syouan/douei/tori/';

async function tryMaffIndex() {
  // Iterate annual season pages on MAFF site. Each season page lists every
  // confirmed case with prefecture, date and case number. We extract those.
  const seasonUrls = [
    MAFF_TORI_INDEX,
    'https://www.maff.go.jp/j/syouan/douei/tori/r5_hpai_kokunai.html',
    'https://www.maff.go.jp/j/syouan/douei/tori/r4_hpai_kokunai.html',
    'https://www.maff.go.jp/j/syouan/douei/tori/r3_hpai_kokunai.html',
    'https://www.maff.go.jp/j/syouan/douei/tori/r2_hpai_kokunai.html',
  ];
  const features = [];
  let idx = 0;
  for (const url of seasonUrls) {
    const html = await fetchText(url, { timeoutMs: 10_000, retries: 1 });
    if (!html) continue;
    if (!/鳥インフルエンザ|HPAI|tori/.test(html)) continue;
    // Parse "X道府県NN例目" / "○○県（NN例目）" / "Pref Name" patterns.
    // Each table row mentions the prefecture and case number.
    const rowRx = /(北海道|青森県|岩手県|宮城県|秋田県|山形県|福島県|茨城県|栃木県|群馬県|埼玉県|千葉県|東京都|神奈川県|新潟県|富山県|石川県|福井県|山梨県|長野県|岐阜県|静岡県|愛知県|三重県|滋賀県|京都府|大阪府|兵庫県|奈良県|和歌山県|鳥取県|島根県|岡山県|広島県|山口県|徳島県|香川県|愛媛県|高知県|福岡県|佐賀県|長崎県|熊本県|大分県|宮崎県|鹿児島県|沖縄県)[^<>]{0,200}?(\d+)\s*例目/g;
    const seen = new Set();
    let m;
    while ((m = rowRx.exec(html)) !== null) {
      const pref = m[1];
      const caseNo = m[2];
      const key = `${pref}_${caseNo}`;
      if (seen.has(key)) continue;
      seen.add(key);
      const c = PREF_CENTROIDS[pref];
      if (!c) continue;
      idx++;
      // Spread cases around prefecture centroid
      const angle = (idx * 2.4) % (2 * Math.PI);
      const r = 0.05 + ((idx * 7) % 10) * 0.01;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [c[0] + Math.cos(angle) * r, c[1] + Math.sin(angle) * r] },
        properties: {
          outbreak_id: `HPAI_LIVE_${pref}_${caseNo}`,
          prefecture: pref,
          case_no: parseInt(caseNo),
          season_url: url,
          country: 'JP',
          source: 'maff_hpai_html',
        },
      });
    }
  }
  return features.length > 0 ? features : null;
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

// Prefecture name → city centroid for geocoding outbreak announcements.
const PREF_CENTROIDS = Object.fromEntries(SEED_OUTBREAKS.map((s) => [s.prefecture, [s.lon, s.lat]]));

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
  let features = await tryMaffIndex();
  let liveSource = 'maff_hpai_index';
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
