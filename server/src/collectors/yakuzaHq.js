/**
 * Yakuza Headquarters Collector
 * NPA-designated bouryokudan (暴力団) HQs and major office locations.
 * Public data: National Police Agency 指定暴力団 list (annually updated).
 */

import { fetchJson } from './_liveHelpers.js';

const NPA_JSON = 'https://www.npa.go.jp/bureau/sosikihanzai/bouryokudan/bou-boutai/bouryokudan-shitei.json';

async function tryNpaList() {
  const data = await fetchJson(NPA_JSON, { timeoutMs: 8000 });
  if (!data || !Array.isArray(data?.items)) return null;
  return data.items.slice(0, 50).map((it, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [it.lon || 139.6917, it.lat || 35.6896] },
    properties: {
      org_id: `NPA_${i + 1}`,
      name: it.name || `Designated org ${i + 1}`,
      designation: it.designation || null,
      members: it.members || null,
      prefecture: it.prefecture || null,
      country: 'JP',
      source: 'npa_shitei',
    },
  }));
}

// Curated from NPA 指定暴力団 list (25 designated organizations) — publicly published HQ addresses
const SEED_ORGS = [
  { name: '六代目山口組 総本部', lat: 34.7264, lon: 135.1839, designation: 'tokutei', members: 3900, prefecture: '兵庫県', hq_city: '神戸市灘区篠原本町' },
  { name: '神戸山口組 総本部', lat: 34.7000, lon: 135.1981, designation: 'tokutei', members: 530, prefecture: '兵庫県', hq_city: '淡路市塩田新島' },
  { name: '絆會', lat: 34.6973, lon: 135.5000, designation: 'shitei', members: 130, prefecture: '大阪府', hq_city: '大阪市西成区' },
  { name: '住吉会 (総本部)', lat: 35.6697, lon: 139.7042, designation: 'shitei', members: 2500, prefecture: '東京都', hq_city: '港区赤坂' },
  { name: '稲川会 (総本部)', lat: 35.6606, lon: 139.7311, designation: 'shitei', members: 3200, prefecture: '東京都', hq_city: '港区六本木' },
  { name: '松葉会 (総本部)', lat: 35.7250, lon: 139.7958, designation: 'shitei', members: 700, prefecture: '東京都', hq_city: '台東区西浅草' },
  { name: '極東会 (総本部)', lat: 35.7094, lon: 139.7050, designation: 'shitei', members: 420, prefecture: '東京都', hq_city: '豊島区西池袋' },
  { name: '会津小鉄会 (総本部)', lat: 34.9847, lon: 135.7636, designation: 'shitei', members: 85, prefecture: '京都府', hq_city: '京都市下京区' },
  { name: '共政会 (総本部)', lat: 34.3900, lon: 132.4700, designation: 'shitei', members: 170, prefecture: '広島県', hq_city: '広島市南区' },
  { name: '合田一家 (総本部)', lat: 33.9578, lon: 130.9406, designation: 'shitei', members: 75, prefecture: '山口県', hq_city: '下関市竹崎町' },
  { name: '小桜一家 (総本部)', lat: 31.5867, lon: 130.5531, designation: 'shitei', members: 100, prefecture: '鹿児島県', hq_city: '鹿児島市甲突町' },
  { name: '浅野組 (総本部)', lat: 34.4936, lon: 133.4111, designation: 'shitei', members: 140, prefecture: '岡山県', hq_city: '笠岡市' },
  { name: '道仁会 (総本部)', lat: 33.3203, lon: 130.5078, designation: 'shitei', members: 410, prefecture: '福岡県', hq_city: '久留米市' },
  { name: '九州誠道会 (浪川会)', lat: 32.7433, lon: 129.8711, designation: 'shitei', members: 260, prefecture: '長崎県', hq_city: '長崎市新地町' },
  { name: '工藤會 (総本部)', lat: 33.8833, lon: 130.8833, designation: 'tokutei', members: 240, prefecture: '福岡県', hq_city: '北九州市小倉北区' },
  { name: '太州会 (総本部)', lat: 33.6450, lon: 130.6983, designation: 'shitei', members: 180, prefecture: '福岡県', hq_city: '田川市' },
  { name: '福博会 (総本部)', lat: 33.5900, lon: 130.4100, designation: 'shitei', members: 120, prefecture: '福岡県', hq_city: '福岡市博多区' },
  { name: '双愛会 (総本部)', lat: 35.6058, lon: 140.1058, designation: 'shitei', members: 160, prefecture: '千葉県', hq_city: '市原市' },
  { name: '東組 (総本部)', lat: 34.6361, lon: 135.5072, designation: 'shitei', members: 130, prefecture: '大阪府', hq_city: '大阪市西成区' },
  { name: '松葉会 旭川支部', lat: 43.7706, lon: 142.3650, designation: 'shitei_sub', members: 50, prefecture: '北海道', hq_city: '旭川市' },
  { name: '侠道会 (総本部)', lat: 34.4075, lon: 133.2053, designation: 'shitei', members: 130, prefecture: '広島県', hq_city: '尾道市山波町' },
  { name: '酒梅組 (総本部)', lat: 34.6361, lon: 135.5072, designation: 'shitei', members: 130, prefecture: '大阪府', hq_city: '大阪市西成区' },
  { name: '任侠山口組 (後の絆會)', lat: 34.6973, lon: 135.5000, designation: 'defunct', members: 0, prefecture: '大阪府', hq_city: '大阪市西成区' },
  { name: '山健組 (六代目山口組傘下)', lat: 34.7200, lon: 135.1900, designation: 'shitei_sub', members: 900, prefecture: '兵庫県', hq_city: '神戸市' },
  { name: '弘道会 (六代目山口組傘下)', lat: 35.1700, lon: 136.9100, designation: 'shitei_sub', members: 1200, prefecture: '愛知県', hq_city: '名古屋市中村区' },
];

function generateSeedData() {
  return SEED_ORGS.map((o, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [o.lon, o.lat] },
    properties: {
      org_id: `YAK_${String(i + 1).padStart(4, '0')}`,
      name: o.name,
      designation: o.designation,
      members_est: o.members,
      prefecture: o.prefecture,
      hq_city: o.hq_city,
      country: 'JP',
      source: 'npa_shitei_seed',
    },
  }));
}

export default async function collectYakuzaHq() {
  let features = await tryNpaList();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'yakuza-hq',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'npa_shitei' : 'npa_shitei_seed',
      description: 'NPA-designated bouryokudan headquarters - public designated organized crime list',
    },
    metadata: {},
  };
}
