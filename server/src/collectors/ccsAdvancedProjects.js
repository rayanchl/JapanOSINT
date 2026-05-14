/**
 * Japanese Advanced CCS Projects (先進的CCS支援事業) — 9 selected projects.
 *
 * 5 domestic + 4 overseas storage projects backed by JOGMEC support, targeting
 * ~20 Mt CO₂/yr storage by 2030. Source list lives at
 *   https://www.jogmec.go.jp/activities/ccs/support-projects/index.html
 *
 * The page is image-heavy with project names and consortia listed in narrative
 * form (and in the figure alt text). We hardcode the 9 projects with cluster-
 * centroid coordinates (the page itself notes the displayed regions are
 * illustrative, not precise locations) and scrape the page only to pick up
 * the page's own title/last-modified for verification.
 */

import { fetchText } from './_liveHelpers.js';

const SOURCE_URL = 'https://www.jogmec.go.jp/activities/ccs/support-projects/index.html';

const SEED_PROJECTS = [
  {
    name_ja: '苫小牧地域CCS', name: 'Tomakomai Area CCS',
    region: 'Hokkaido',  country: 'JP', storage_type: 'offshore',
    operators: ['JAPEX', 'idemitsu', 'Hokkaido Electric Power'],
    lat: 42.65,  lon: 141.65,
  },
  {
    name_ja: '日本海側東北地方CCS', name: 'Sea-of-Japan Tohoku CCS',
    region: 'Tohoku',    country: 'JP', storage_type: 'offshore',
    operators: ['INPEX', 'Nippon Steel', 'Taiheiyo Cement', 'MHI', 'Taisei', 'Itochu'],
    lat: 40.20,  lon: 139.30,
  },
  {
    name_ja: '東新潟地域CCS', name: 'East Niigata Area CCS',
    region: 'Niigata',   country: 'JP', storage_type: 'onshore_depleted_gas',
    operators: ['JAPEX', 'Tohoku Electric Power', 'Mitsubishi Gas Chemical', 'Hokuetsu Corporation'],
    lat: 37.92,  lon: 139.10,
  },
  {
    name_ja: '首都圏CCS', name: 'Capital Region CCS',
    region: 'Kanto',     country: 'JP', storage_type: 'offshore',
    operators: ['INPEX', 'Nippon Steel', 'Kanto Natural Gas Development'],
    lat: 35.55,  lon: 140.15,
  },
  {
    name_ja: '九州西部沖CCS', name: 'West Kyushu Offshore CCS',
    region: 'Kyushu',    country: 'JP', storage_type: 'offshore',
    operators: ['ENEOS', 'JX Nippon Oil & Gas Exploration', 'J-POWER'],
    lat: 33.10,  lon: 129.40,
  },
  {
    name_ja: 'マレー半島沖北部CCS', name: 'Northern Malay Peninsula Offshore CCS',
    region: 'Malaysia',  country: 'MY', storage_type: 'offshore',
    operators: ['Mitsubishi Corporation', 'ENEOS', 'JX', 'PETRONAS', 'Nippon Shokubai', 'JFE', 'COSMO'],
    lat: 5.60,   lon: 102.50,
  },
  {
    name_ja: 'サラワク沖CCS', name: 'Sarawak Offshore CCS',
    region: 'Sarawak',   country: 'MY', storage_type: 'offshore',
    operators: ['JAPEX', 'JGC', 'K LINE', 'PETRONAS', 'JFE', 'Mitsubishi Gas Chemical', 'NGL', 'Chugoku Electric', 'Mitsubishi Chemical'],
    lat: 4.50,   lon: 112.50,
  },
  {
    name_ja: 'マレー半島沖南部CCS', name: 'Southern Malay Peninsula Offshore CCS',
    region: 'Malaysia',  country: 'MY', storage_type: 'offshore',
    operators: ['Mitsui & Co.', 'Kansai Electric', 'COSMO', 'Chugoku Electric', 'J-POWER', 'Kyushu Electric', 'RESONAC', 'UBE Mitsubishi Cement'],
    lat: 2.80,   lon: 103.80,
  },
  {
    name_ja: '大洋州CCS', name: 'Oceania CCS',
    region: 'Oceania',   country: 'AU', storage_type: 'offshore',
    operators: ['Mitsubishi Corporation', 'Nippon Steel', 'ExxonMobil', 'Mitsubishi Chemical Group', 'Mitsubishi Corporation Clean Energy'],
    lat: -20.0,  lon: 132.0,
  },
];

async function verifyAgainstJogmec() {
  const html = await fetchText(SOURCE_URL, { timeoutMs: 12_000, retries: 1 });
  if (!html) return { matched: 0, verifiedAt: null };
  let matched = 0;
  for (const p of SEED_PROJECTS) {
    if (html.includes(p.name_ja)) matched++;
  }
  return { matched, verifiedAt: new Date().toISOString() };
}

export default async function collectCcsAdvancedProjects() {
  const verify = await verifyAgainstJogmec();
  const live = verify.matched >= SEED_PROJECTS.length / 2;

  const features = SEED_PROJECTS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      project_id: `CCS_${String(i + 1).padStart(3, '0')}`,
      name: p.name,
      name_ja: p.name_ja,
      region: p.region,
      country: p.country,
      storage_type: p.storage_type,
      operators: p.operators,
      operator_lead: p.operators[0],
      verified_at: live ? verify.verifiedAt : null,
      source: live ? 'ccs_advanced_jogmec' : 'ccs_advanced_seed',
      source_url: SOURCE_URL,
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'ccs-projects',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      matched_against_jogmec: verify.matched,
      description: 'JOGMEC Advanced CCS Projects (先進的CCS支援事業) — 5 domestic + 4 overseas, ~20 Mt CO₂/yr by 2030',
    },
  };
}
