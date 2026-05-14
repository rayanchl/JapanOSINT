/**
 * Vending Machines Collector
 * Japan has ~2.3M vending machines — densest per-capita on earth.
 * Live: OSM Overpass `amenity=vending_machine` (sample a subset since
 * a nationwide pull would exceed Overpass limits). Seed is curated
 * density zones anchored to major transit hubs and urban districts.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  // Constrained to a few bbox samples around Tokyo / Osaka / Nagoya stations
  // so the query stays under Overpass limits. Returns representative points.
  return fetchOverpass(
    'node["amenity"="vending_machine"](35.6,139.6,35.8,139.85);node["amenity"="vending_machine"](34.6,135.4,34.75,135.6);node["amenity"="vending_machine"](35.1,136.8,35.25,137.0);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        vm_id: `OSM_${el.id}`,
        vending: el.tags?.vending || 'drinks',
        operator: el.tags?.operator || null,
        brand: el.tags?.brand || null,
        payment: el.tags?.['payment:coins'] === 'yes' ? 'coin' : (el.tags?.['payment:cards'] === 'yes' ? 'card' : 'coin'),
        indoor: el.tags?.indoor === 'yes',
        source: 'osm_overpass',
      },
    }),
  );
}

// Seed: density hotspots — one point per cluster with estimated count.
const SEED_ZONES = [
  // Tokyo 23 wards — densest zones
  { name: 'Shinjuku Station West Exit', lat: 35.6919, lon: 139.6989, prefecture: '東京都', city: '新宿区', density: 380, zone: 'transit_hub' },
  { name: 'Shibuya Scramble', lat: 35.6595, lon: 139.7006, prefecture: '東京都', city: '渋谷区', density: 320, zone: 'transit_hub' },
  { name: 'Tokyo Station Marunouchi', lat: 35.6812, lon: 139.7671, prefecture: '東京都', city: '千代田区', density: 280, zone: 'transit_hub' },
  { name: 'Ikebukuro East Exit', lat: 35.7295, lon: 139.7109, prefecture: '東京都', city: '豊島区', density: 260, zone: 'transit_hub' },
  { name: 'Akihabara Electric Town', lat: 35.6984, lon: 139.7731, prefecture: '東京都', city: '千代田区', density: 240, zone: 'entertainment' },
  { name: 'Asakusa Sensoji', lat: 35.7148, lon: 139.7967, prefecture: '東京都', city: '台東区', density: 180, zone: 'tourist' },
  { name: 'Ueno Ameyoko', lat: 35.7109, lon: 139.7747, prefecture: '東京都', city: '台東区', density: 200, zone: 'market' },
  { name: 'Ginza 4-chome', lat: 35.6717, lon: 139.7650, prefecture: '東京都', city: '中央区', density: 150, zone: 'commercial' },
  { name: 'Roppongi Hills', lat: 35.6604, lon: 139.7292, prefecture: '東京都', city: '港区', density: 140, zone: 'commercial' },
  { name: 'Shinagawa Station', lat: 35.6284, lon: 139.7387, prefecture: '東京都', city: '港区', density: 260, zone: 'transit_hub' },
  { name: 'Ebisu Station', lat: 35.6467, lon: 139.7100, prefecture: '東京都', city: '渋谷区', density: 160, zone: 'commercial' },
  { name: 'Nakano Broadway', lat: 35.7077, lon: 139.6656, prefecture: '東京都', city: '中野区', density: 170, zone: 'market' },
  { name: 'Kichijoji Station', lat: 35.7028, lon: 139.5792, prefecture: '東京都', city: '武蔵野市', density: 180, zone: 'commercial' },
  { name: 'Harajuku Takeshita-dori', lat: 35.6713, lon: 139.7027, prefecture: '東京都', city: '渋谷区', density: 190, zone: 'tourist' },
  { name: 'Odaiba Seaside Park', lat: 35.6270, lon: 139.7733, prefecture: '東京都', city: '港区', density: 130, zone: 'tourist' },

  // Osaka
  { name: 'Umeda Station (Osaka)', lat: 34.7025, lon: 135.4959, prefecture: '大阪府', city: '大阪市北区', density: 340, zone: 'transit_hub' },
  { name: 'Namba Station', lat: 34.6659, lon: 135.5007, prefecture: '大阪府', city: '大阪市中央区', density: 300, zone: 'transit_hub' },
  { name: 'Shinsaibashi-suji', lat: 34.6733, lon: 135.5014, prefecture: '大阪府', city: '大阪市中央区', density: 220, zone: 'commercial' },
  { name: 'Dotonbori', lat: 34.6688, lon: 135.5019, prefecture: '大阪府', city: '大阪市中央区', density: 200, zone: 'entertainment' },
  { name: 'Tennoji Station', lat: 34.6466, lon: 135.5137, prefecture: '大阪府', city: '大阪市天王寺区', density: 210, zone: 'transit_hub' },
  { name: 'Shin-Osaka Station', lat: 34.7336, lon: 135.5003, prefecture: '大阪府', city: '大阪市淀川区', density: 250, zone: 'transit_hub' },
  { name: 'Kyobashi Station', lat: 34.6970, lon: 135.5342, prefecture: '大阪府', city: '大阪市都島区', density: 180, zone: 'transit_hub' },

  // Kyoto
  { name: 'Kyoto Station Karasuma', lat: 34.9858, lon: 135.7588, prefecture: '京都府', city: '京都市下京区', density: 240, zone: 'transit_hub' },
  { name: 'Gion Shijo', lat: 35.0037, lon: 135.7755, prefecture: '京都府', city: '京都市東山区', density: 160, zone: 'tourist' },
  { name: 'Arashiyama', lat: 35.0094, lon: 135.6778, prefecture: '京都府', city: '京都市右京区', density: 120, zone: 'tourist' },
  { name: 'Kawaramachi', lat: 35.0037, lon: 135.7692, prefecture: '京都府', city: '京都市中京区', density: 180, zone: 'commercial' },

  // Nagoya
  { name: 'Nagoya Station Sakura-dori', lat: 35.1709, lon: 136.8815, prefecture: '愛知県', city: '名古屋市中村区', density: 280, zone: 'transit_hub' },
  { name: 'Sakae', lat: 35.1710, lon: 136.9083, prefecture: '愛知県', city: '名古屋市中区', density: 220, zone: 'commercial' },
  { name: 'Osu Shopping District', lat: 35.1592, lon: 136.9042, prefecture: '愛知県', city: '名古屋市中区', density: 170, zone: 'market' },

  // Fukuoka
  { name: 'Hakata Station', lat: 33.5903, lon: 130.4206, prefecture: '福岡県', city: '福岡市博多区', density: 260, zone: 'transit_hub' },
  { name: 'Tenjin', lat: 33.5911, lon: 130.3989, prefecture: '福岡県', city: '福岡市中央区', density: 220, zone: 'commercial' },
  { name: 'Nakasu', lat: 33.5925, lon: 130.4072, prefecture: '福岡県', city: '福岡市博多区', density: 180, zone: 'entertainment' },

  // Sapporo
  { name: 'Sapporo Station', lat: 43.0686, lon: 141.3507, prefecture: '北海道', city: '札幌市北区', density: 220, zone: 'transit_hub' },
  { name: 'Susukino', lat: 43.0551, lon: 141.3544, prefecture: '北海道', city: '札幌市中央区', density: 200, zone: 'entertainment' },
  { name: 'Odori Park', lat: 43.0606, lon: 141.3544, prefecture: '北海道', city: '札幌市中央区', density: 150, zone: 'commercial' },

  // Sendai
  { name: 'Sendai Station', lat: 38.2600, lon: 140.8824, prefecture: '宮城県', city: '仙台市青葉区', density: 230, zone: 'transit_hub' },
  { name: 'Kokubuncho', lat: 38.2664, lon: 140.8719, prefecture: '宮城県', city: '仙台市青葉区', density: 180, zone: 'entertainment' },

  // Hiroshima
  { name: 'Hiroshima Station', lat: 34.3976, lon: 132.4757, prefecture: '広島県', city: '広島市南区', density: 210, zone: 'transit_hub' },
  { name: 'Hondori Shopping Arcade', lat: 34.3955, lon: 132.4585, prefecture: '広島県', city: '広島市中区', density: 160, zone: 'commercial' },

  // Yokohama
  { name: 'Yokohama Station West', lat: 35.4660, lon: 139.6222, prefecture: '神奈川県', city: '横浜市西区', density: 280, zone: 'transit_hub' },
  { name: 'Minatomirai', lat: 35.4575, lon: 139.6314, prefecture: '神奈川県', city: '横浜市西区', density: 180, zone: 'tourist' },
  { name: 'Chinatown Yokohama', lat: 35.4437, lon: 139.6459, prefecture: '神奈川県', city: '横浜市中区', density: 170, zone: 'tourist' },

  // Kobe
  { name: 'Sannomiya Station', lat: 34.6946, lon: 135.1952, prefecture: '兵庫県', city: '神戸市中央区', density: 240, zone: 'transit_hub' },
  { name: 'Kobe Harborland', lat: 34.6826, lon: 135.1825, prefecture: '兵庫県', city: '神戸市中央区', density: 160, zone: 'tourist' },

  // Niigata
  { name: 'Niigata Station', lat: 37.9122, lon: 139.0614, prefecture: '新潟県', city: '新潟市中央区', density: 180, zone: 'transit_hub' },

  // Okinawa
  { name: 'Kokusai-dori Naha', lat: 26.2151, lon: 127.6878, prefecture: '沖縄県', city: '那覇市', density: 150, zone: 'tourist' },

  // Remote / curiosity VMs — Japan is famous for middle-of-nowhere VMs
  { name: 'Mt. Fuji 5th Station', lat: 35.3953, lon: 138.7406, prefecture: '山梨県', city: '富士吉田市', density: 40, zone: 'remote' },
  { name: 'Oirase Stream', lat: 40.5414, lon: 140.9242, prefecture: '青森県', city: '十和田市', density: 15, zone: 'remote' },
  { name: 'Kumano Kodo', lat: 33.8403, lon: 135.7731, prefecture: '和歌山県', city: '田辺市', density: 20, zone: 'remote' },
];

function generateSeedData() {
  return SEED_ZONES.map((z, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [z.lon, z.lat] },
    properties: {
      vm_id: `VM_ZONE_${i + 1}`,
      name: z.name,
      zone_type: z.zone,
      vm_count_est: z.density,
      prefecture: z.prefecture,
      city: z.city,
      country: 'JP',
      source: 'density_seed',
    },
  }));
}

export default async function collectVendingMachines() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'vending-machines',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'density_seed',
      description: 'Japanese vending machine locations and density zones (~2.3M nationwide)',
    },
  };
}
