/**
 * 5G Coverage Collector
 * MIC 5G base station registry CSV (when reachable). Falls back to seed of
 * 5G coverage zones across major Japanese cities.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["tower:type"="communication"]["communication:mobile_phone"="yes"](area.jp);node["man_made"="tower"]["tower:type"="communication"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        cell_id: `5G_LIVE_${String(i + 1).padStart(5, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Tower ${el.id}`,
        operator: el.tags?.operator || 'unknown',
        tech: el.tags?.['communication:mobile_phone'] === 'yes' ? '5G/LTE' : 'communication',
        bands: el.tags?.['communication:frequencies'] || null,
        country: 'JP',
        source: '5g_coverage_live',
      },
    })
  );
}

const SEED_5G = [
  // Tokyo metropolitan
  { name: '東京 都心 5G カバー', lat: 35.6896, lon: 139.6917, operator: 'NTT Docomo', tech: '5G NSA+SA', bands: 'n78,n257' },
  { name: '東京 都心 KDDI 5G', lat: 35.6896, lon: 139.6917, operator: 'KDDI au', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  { name: '東京 都心 SoftBank 5G', lat: 35.6896, lon: 139.6917, operator: 'SoftBank', tech: '5G NSA', bands: 'n77,n257' },
  { name: '東京 都心 Rakuten 5G', lat: 35.6896, lon: 139.6917, operator: 'Rakuten Mobile', tech: '5G NSA+SA', bands: 'n77,n257' },
  { name: '渋谷 5G ホットスポット', lat: 35.6614, lon: 139.7041, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  { name: '新宿 5G ホットスポット', lat: 35.6939, lon: 139.7036, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  { name: '六本木 5G ホットスポット', lat: 35.6627, lon: 139.7311, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  { name: '池袋 5G エリア', lat: 35.7295, lon: 139.7109, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n77,n78' },
  { name: '銀座 5G エリア', lat: 35.6717, lon: 139.7650, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n77,n78' },
  { name: '秋葉原 5G エリア', lat: 35.6983, lon: 139.7731, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n77,n78' },
  { name: '東京駅 5G エリア', lat: 35.6812, lon: 139.7670, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  { name: '品川駅 5G エリア', lat: 35.6284, lon: 139.7387, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  // Yokohama
  { name: '横浜駅 5G エリア', lat: 35.4659, lon: 139.6224, operator: 'Multi-carrier', tech: '5G NSA+SA', bands: 'n77,n78' },
  { name: 'みなとみらい 5G エリア', lat: 35.4561, lon: 139.6311, operator: 'Multi-carrier', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  // Kawasaki
  { name: '川崎駅 5G エリア', lat: 35.5311, lon: 139.6967, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Chiba
  { name: '千葉駅 5G エリア', lat: 35.6133, lon: 140.1133, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: '幕張新都心 5G エリア', lat: 35.6489, lon: 140.0339, operator: 'Multi-carrier', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  // Osaka
  { name: '大阪 都心 5G カバー', lat: 34.6937, lon: 135.5023, operator: 'NTT Docomo', tech: '5G NSA+SA', bands: 'n78,n257' },
  { name: '大阪 KDDI 5G', lat: 34.6937, lon: 135.5023, operator: 'KDDI au', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  { name: '梅田 5G ホットスポット', lat: 34.7028, lon: 135.4961, operator: 'Multi-carrier', tech: '5G mmWave', bands: 'n257' },
  { name: '難波 5G エリア', lat: 34.6650, lon: 135.5009, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n77,n78' },
  { name: '天王寺 5G エリア', lat: 34.6478, lon: 135.5137, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Kyoto
  { name: '京都駅 5G エリア', lat: 34.9858, lon: 135.7589, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Kobe
  { name: '三宮 5G エリア', lat: 34.6939, lon: 135.1953, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Nagoya
  { name: '名古屋駅 5G エリア', lat: 35.1700, lon: 136.8800, operator: 'Multi-carrier', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  { name: '栄 5G エリア', lat: 35.1700, lon: 136.9100, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Sapporo
  { name: '札幌駅 5G エリア', lat: 43.0686, lon: 141.3506, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: 'すすきの 5G エリア', lat: 43.0561, lon: 141.3528, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Sendai
  { name: '仙台駅 5G エリア', lat: 38.2606, lon: 140.8819, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Fukuoka
  { name: '博多駅 5G エリア', lat: 33.5901, lon: 130.4205, operator: 'Multi-carrier', tech: '5G NSA+SA', bands: 'n77,n78,n257' },
  { name: '天神 5G エリア', lat: 33.5928, lon: 130.4019, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Hiroshima
  { name: '広島駅 5G エリア', lat: 34.3978, lon: 132.4753, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Naha
  { name: '那覇国際通り 5G エリア', lat: 26.2150, lon: 127.6800, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  // Other secondary
  { name: '岡山駅 5G エリア', lat: 34.6661, lon: 133.9181, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: '熊本駅 5G エリア', lat: 32.7889, lon: 130.6889, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: '鹿児島中央駅 5G エリア', lat: 31.5836, lon: 130.5414, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: '高松駅 5G エリア', lat: 34.3500, lon: 134.0469, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
  { name: '長崎駅 5G エリア', lat: 32.7522, lon: 129.8794, operator: 'Multi-carrier', tech: '5G NSA', bands: 'n78' },
];

function generateSeedData() {
  return SEED_5G.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      cell_id: `5G_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      operator: s.operator,
      tech: s.tech,
      bands: s.bands,
      country: 'JP',
      source: 'mic_5g_seed',
    },
  }));
}

export default async function collect5gCoverage() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: '5g_coverage',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: '5G NSA/SA coverage zones (Docomo, KDDI au, SoftBank, Rakuten Mobile) — n77/n78 sub-6 + n257 mmWave',
    },
    metadata: {},
  };
}
