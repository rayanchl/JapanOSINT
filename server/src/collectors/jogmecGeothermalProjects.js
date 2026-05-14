/**
 * JOGMEC Geothermal Model Districts — 3 designated + 5 case-study regions.
 *
 * Source: https://geothermal-model.jogmec.go.jp/  (JOGMEC's geothermal
 * project portal; pages are narrative HTML, not structured tables, so we
 * only scrape the page title to confirm each district page is still up.)
 *
 * Coordinates are town-hall centroids; installed_capacity_mw aggregates
 * the operating geothermal plants known to be in the district from
 * publicly disclosed facility data — these stay in the seed because the
 * JOGMEC pages don't list capacity in a parseable form.
 */

import { fetchText } from './_liveHelpers.js';

const BASE = 'https://geothermal-model.jogmec.go.jp';

const SEED_DISTRICTS = [
  // 3 designated model districts
  { slug: 'mori-machi',     district_type: 'model',
    name_ja: '北海道茅部郡森町', name: 'Mori, Hokkaido',
    prefecture: '北海道', town: '森町',
    lat: 42.106, lon: 140.583,
    installed_capacity_mw: 50,
    notable_plants: ['森地熱発電所 (Mori, 50 MW)'],
    path: '/model/mori-machi/' },
  { slug: 'hachimantai-shi', district_type: 'model',
    name_ja: '岩手県八幡平市', name: 'Hachimantai, Iwate',
    prefecture: '岩手県', town: '八幡平市',
    lat: 39.927, lon: 140.987,
    installed_capacity_mw: 103.5,
    notable_plants: ['松川地熱発電所 (Matsukawa, 23.5 MW)', '葛根田地熱発電所 (Kakkonda, 80 MW)'],
    path: '/model/hachimantai-shi/' },
  { slug: 'yuzawa-shi',     district_type: 'model',
    name_ja: '秋田県湯沢市', name: 'Yuzawa, Akita',
    prefecture: '秋田県', town: '湯沢市',
    lat: 39.164, lon: 140.495,
    installed_capacity_mw: 73.7,
    notable_plants: ['上の岱地熱発電所 (Uenotai, 27.5 MW)', '山葵沢地熱発電所 (Wasabizawa, 46.2 MW)'],
    path: '/model/yuzawa-shi/' },
  // 5 case-study regions
  { slug: 'teshikaga-chou', district_type: 'case_study',
    name_ja: '北海道弟子屈町', name: 'Teshikaga, Hokkaido',
    prefecture: '北海道', town: '弟子屈町',
    lat: 43.484, lon: 144.460,
    installed_capacity_mw: null,
    notable_plants: ['Mashu / Kawayu exploration district'],
    path: '/case/teshikaga-chou/' },
  { slug: 'osaki-shi',       district_type: 'case_study',
    name_ja: '宮城県大崎市', name: 'Osaki, Miyagi',
    prefecture: '宮城県', town: '大崎市',
    lat: 38.730, lon: 140.738,
    installed_capacity_mw: null,
    notable_plants: ['鳴子温泉郷 (Naruko hot-spring district)'],
    path: '/case/osaki-shi/' },
  { slug: 'fukushima-shi',   district_type: 'case_study',
    name_ja: '福島県福島市', name: 'Fukushima City, Fukushima',
    prefecture: '福島県', town: '福島市',
    lat: 37.760, lon: 140.473,
    installed_capacity_mw: 0.4,
    notable_plants: ['土湯温泉バイナリー発電所 (Tsuchiyu binary, 0.4 MW)'],
    path: '/case/fukushima-shi/' },
  { slug: 'kokonoe-machi',   district_type: 'case_study',
    name_ja: '大分県九重町', name: 'Kokonoe, Oita',
    prefecture: '大分県', town: '九重町',
    lat: 33.183, lon: 131.215,
    installed_capacity_mw: 110,
    notable_plants: ['八丁原地熱発電所 (Hatchobaru, 110 MW — largest in Japan)'],
    path: '/case/kokonoe-machi/' },
  { slug: 'oguni-machi',     district_type: 'case_study',
    name_ja: '熊本県小国町', name: 'Oguni, Kumamoto',
    prefecture: '熊本県', town: '小国町',
    lat: 33.110, lon: 131.080,
    installed_capacity_mw: 50,
    notable_plants: ['わいた地熱発電所 (Waita, ~50 MW)'],
    path: '/case/oguni-machi/' },
];

async function probeJogmecPage(path) {
  const url = `${BASE}${path}`;
  const html = await fetchText(url, { timeoutMs: 10_000, retries: 1 });
  if (!html) return null;
  const titleMatch = html.match(/<title>([\s\S]*?)<\/title>/);
  return {
    page_title: titleMatch ? titleMatch[1].trim() : null,
    source_url: url,
  };
}

export default async function collectJogmecGeothermalProjects() {
  const probes = await Promise.all(SEED_DISTRICTS.map((d) => probeJogmecPage(d.path)));
  let liveCount = 0;
  const features = SEED_DISTRICTS.map((d, i) => {
    const probe = probes[i];
    if (probe) liveCount++;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
      properties: {
        project_id: `GEO_${String(i + 1).padStart(3, '0')}`,
        name: d.name,
        name_ja: d.name_ja,
        district_type: d.district_type,
        prefecture: d.prefecture,
        town: d.town,
        installed_capacity_mw: d.installed_capacity_mw,
        notable_plants: d.notable_plants,
        page_title: probe?.page_title ?? null,
        country: 'JP',
        source: probe ? 'jogmec_geothermal_live' : 'jogmec_geothermal_seed_fallback',
        source_url: probe?.source_url ?? `${BASE}${d.path}`,
      },
    };
  });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'geothermal-projects',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: liveCount > 0,
      verified_pages: liveCount,
      description: 'JOGMEC geothermal model districts (3 designated + 5 case-study regions)',
    },
  };
}
