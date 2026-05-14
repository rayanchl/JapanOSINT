/**
 * EV Charging Stations Collector
 * Maps EV charging infrastructure across Japan:
 * - CHAdeMO rapid chargers
 * - CCS/Combo chargers
 * - Type 1/Type 2 AC chargers
 * - Tesla Superchargers
 * Uses OpenChargeMap API when available
 */

import { fetchOverpass, fetchJson } from './_liveHelpers.js';
import { getEnv } from '../utils/credentials.js';

const openChargeMapKey = () => getEnv(null, 'OPENCHARGEMAP_KEY') || '';

const CONNECTOR_TYPE_MAP = {
  2: 'CHAdeMO',
  25: 'Type 2',
  32: 'CCS',
  33: 'CCS2',
  30: 'Tesla',
};

async function tryOpenChargeMap() {
  let url = 'https://api.openchargemap.io/v3/poi/?output=json&countrycode=JP&maxresults=500&compact=true&verbose=false';
  const key = openChargeMapKey();
  if (key) url += `&key=${key}`;
  const data = await fetchJson(url);
  if (!Array.isArray(data)) return [];
  return data
    .filter((poi) => poi.AddressInfo?.Latitude && poi.AddressInfo?.Longitude)
    .map((poi) => {
      const connections = poi.Connections || [];
      const connectorTypes = connections
        .map((c) => CONNECTOR_TYPE_MAP[c.ConnectionTypeID] || `Type_${c.ConnectionTypeID}`)
        .filter(Boolean);
      const powerKw = Math.max(0, ...connections.map((c) => c.PowerKW || 0));
      const usageCost = poi.UsageCost || '';
      return {
        type: 'Feature',
        geometry: {
          type: 'Point',
          coordinates: [poi.AddressInfo.Longitude, poi.AddressInfo.Latitude],
        },
        properties: {
          charger_id: `OCM_${poi.ID}`,
          name: poi.AddressInfo.Title || 'Unknown Station',
          operator: poi.OperatorInfo?.Title || 'unknown',
          address: poi.AddressInfo.AddressLine1 || '',
          connector_types: connectorTypes,
          power_kw: powerKw || null,
          num_ports: connections.length,
          is_rapid: powerKw >= 50,
          is_free: usageCost === '' || /free/i.test(usageCost),
          network: poi.OperatorInfo?.Title || 'unknown',
          status: poi.StatusType?.Title || 'unknown',
          country: 'JP',
          source: 'openchargemap',
        },
      };
    });
}

async function tryOSMChargers() {
  return fetchOverpass(
    'node["amenity"="charging_station"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        charger_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Charging station ${i + 1}`,
        operator: el.tags?.operator || 'unknown',
        address: el.tags?.['addr:full'] || el.tags?.['addr:street'] || '',
        connector_types: [
          el.tags?.['socket:chademo'] === 'yes' && 'CHAdeMO',
          el.tags?.['socket:type2'] === 'yes' && 'Type 2',
          el.tags?.['socket:type2_combo'] === 'yes' && 'CCS',
          el.tags?.['socket:tesla_supercharger'] === 'yes' && 'Tesla',
        ].filter(Boolean),
        power_kw: parseFloat(el.tags?.['charging_station:output']) || null,
        num_ports: parseInt(el.tags?.capacity, 10) || null,
        is_rapid: parseFloat(el.tags?.['charging_station:output']) >= 50,
        is_free: el.tags?.fee === 'no',
        network: el.tags?.network || el.tags?.operator || 'unknown',
        status: 'operational',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

/* ---------- seed data ---------- */
const EV_CHARGERS = [
  // Highway SA/PA CHAdeMO – NEXCO East / Central / West
  { name: '海老名SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO East', prefecture: '神奈川県', lat: 35.4294, lon: 139.3911 },
  { name: '足柄SA (下り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO Central', prefecture: '静岡県', lat: 35.2962, lon: 138.9961 },
  { name: '富士川SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO Central', prefecture: '静岡県', lat: 35.1550, lon: 138.6200 },
  { name: '浜松SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO Central', prefecture: '静岡県', lat: 34.7717, lon: 137.7211 },
  { name: '草津PA (下り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO West', prefecture: '滋賀県', lat: 35.0133, lon: 135.9608 },
  { name: '吹田SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO West', prefecture: '大阪府', lat: 34.7711, lon: 135.5178 },
  { name: '多賀SA (下り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO Central', prefecture: '滋賀県', lat: 35.2467, lon: 136.2886 },
  { name: '刈谷PA (下り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO Central', prefecture: '愛知県', lat: 34.9889, lon: 137.0017 },
  { name: '大津SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO West', prefecture: '滋賀県', lat: 34.9833, lon: 135.8833 },
  { name: '佐野SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO East', prefecture: '栃木県', lat: 36.3133, lon: 139.5781 },
  { name: '蓮田SA (上り) 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'NEXCO East', prefecture: '埼玉県', lat: 35.9667, lon: 139.6500 },
  { name: '三方原PA 急速充電器', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'NEXCO Central', prefecture: '静岡県', lat: 34.7833, lon: 137.7333 },

  // Convenience store chargers
  { name: 'セブン-イレブン 東京六本木店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'e-Mobility Power', prefecture: '東京都', lat: 35.6627, lon: 139.7319 },
  { name: 'セブン-イレブン 横浜みなとみらい店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'e-Mobility Power', prefecture: '神奈川県', lat: 35.4558, lon: 139.6328 },
  { name: 'セブン-イレブン 大阪梅田店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'e-Mobility Power', prefecture: '大阪府', lat: 34.7025, lon: 135.4961 },
  { name: 'ファミリーマート 名古屋栄店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 20, num_ports: 1, is_rapid: false, network: 'e-Mobility Power', prefecture: '愛知県', lat: 35.1683, lon: 136.9064 },
  { name: 'ファミリーマート 福岡天神店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 20, num_ports: 1, is_rapid: false, network: 'e-Mobility Power', prefecture: '福岡県', lat: 33.5897, lon: 130.3989 },
  { name: 'ローソン 札幌駅前店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'e-Mobility Power', prefecture: '北海道', lat: 43.0686, lon: 141.3508 },
  { name: 'ローソン 仙台駅東口店 EV充電', operator: 'e-Mobility Power', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'e-Mobility Power', prefecture: '宮城県', lat: 38.2600, lon: 140.8828 },

  // Nissan dealer CHAdeMO
  { name: '日産 東京本社ギャラリー 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 2, is_rapid: false, network: 'Nissan', prefecture: '神奈川県', lat: 35.4756, lon: 139.6325 },
  { name: '日産 大阪なんば店 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: 'Nissan', prefecture: '大阪府', lat: 34.6603, lon: 135.5014 },
  { name: '日産 名古屋中央店 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: 'Nissan', prefecture: '愛知県', lat: 35.1706, lon: 136.9083 },
  { name: '日産 福岡東店 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: 'Nissan', prefecture: '福岡県', lat: 33.6211, lon: 130.4417 },
  { name: '日産 広島南店 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: 'Nissan', prefecture: '広島県', lat: 34.3653, lon: 132.4553 },
  { name: '日産 札幌中央店 急速充電', operator: '日産自動車', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: 'Nissan', prefecture: '北海道', lat: 43.0550, lon: 141.3478 },

  // Toyota dealer
  { name: 'トヨタ 東京トヨペット渋谷店 充電', operator: 'トヨタ自動車', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'Toyota', prefecture: '東京都', lat: 35.6581, lon: 139.7017 },
  { name: 'トヨタ 愛知トヨタ本店 充電', operator: 'トヨタ自動車', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'Toyota', prefecture: '愛知県', lat: 35.1681, lon: 136.9319 },
  { name: 'トヨタ 大阪トヨペット本店 充電', operator: 'トヨタ自動車', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: 'Toyota', prefecture: '大阪府', lat: 34.6869, lon: 135.5200 },

  // Tesla Superchargers
  { name: 'Tesla Supercharger 東京六本木', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 8, is_rapid: true, network: 'Tesla Supercharger', prefecture: '東京都', lat: 35.6600, lon: 139.7294 },
  { name: 'Tesla Supercharger 大阪梅田', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 8, is_rapid: true, network: 'Tesla Supercharger', prefecture: '大阪府', lat: 34.7050, lon: 135.4983 },
  { name: 'Tesla Supercharger 名古屋', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 6, is_rapid: true, network: 'Tesla Supercharger', prefecture: '愛知県', lat: 35.1700, lon: 136.8817 },
  { name: 'Tesla Supercharger 箱根', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 8, is_rapid: true, network: 'Tesla Supercharger', prefecture: '神奈川県', lat: 35.2328, lon: 139.1069 },
  { name: 'Tesla Supercharger 御殿場', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 8, is_rapid: true, network: 'Tesla Supercharger', prefecture: '静岡県', lat: 35.3089, lon: 138.9350 },
  { name: 'Tesla Supercharger 京都', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 6, is_rapid: true, network: 'Tesla Supercharger', prefecture: '京都府', lat: 34.9900, lon: 135.7567 },
  { name: 'Tesla Supercharger 福岡', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 6, is_rapid: true, network: 'Tesla Supercharger', prefecture: '福岡県', lat: 33.5903, lon: 130.4017 },
  { name: 'Tesla Supercharger 仙台', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 4, is_rapid: true, network: 'Tesla Supercharger', prefecture: '宮城県', lat: 38.2614, lon: 140.8694 },
  { name: 'Tesla Supercharger 広島', operator: 'Tesla', connector_type: 'Tesla', power_kw: 250, num_ports: 4, is_rapid: true, network: 'Tesla Supercharger', prefecture: '広島県', lat: 34.3956, lon: 132.4594 },
  { name: 'Tesla Destination 軽井沢', operator: 'Tesla', connector_type: 'Tesla', power_kw: 22, num_ports: 4, is_rapid: false, network: 'Tesla Destination', prefecture: '長野県', lat: 36.3481, lon: 138.6350 },

  // AEON Mall chargers
  { name: 'イオンモール幕張新都心 EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '千葉県', lat: 35.6539, lon: 140.0331 },
  { name: 'イオンモール岡崎 EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '愛知県', lat: 34.9217, lon: 137.1433 },
  { name: 'イオンモール大日 EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '大阪府', lat: 34.7561, lon: 135.5728 },
  { name: 'イオンモール札幌発寒 EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '北海道', lat: 43.0897, lon: 141.2867 },
  { name: 'イオンモール鹿児島 EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '鹿児島県', lat: 31.5239, lon: 130.5356 },
  { name: 'イオンモール沖縄ライカム EV充電', operator: 'WAON充電', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 2, is_rapid: true, network: 'AEON', prefecture: '沖縄県', lat: 26.3381, lon: 127.7694 },

  // IKEA chargers
  { name: 'IKEA Tokyo-Bay EV充電', operator: 'IKEA', connector_type: 'CCS', power_kw: 50, num_ports: 2, is_rapid: true, network: 'IKEA', prefecture: '千葉県', lat: 35.6756, lon: 139.9856 },
  { name: 'IKEA 新三郷 EV充電', operator: 'IKEA', connector_type: 'CCS', power_kw: 50, num_ports: 2, is_rapid: true, network: 'IKEA', prefecture: '埼玉県', lat: 35.8397, lon: 139.8672 },
  { name: 'IKEA 長久手 EV充電', operator: 'IKEA', connector_type: 'CCS', power_kw: 50, num_ports: 2, is_rapid: true, network: 'IKEA', prefecture: '愛知県', lat: 35.1831, lon: 137.0500 },
  { name: 'IKEA 鶴浜 EV充電', operator: 'IKEA', connector_type: 'CCS', power_kw: 50, num_ports: 2, is_rapid: true, network: 'IKEA', prefecture: '大阪府', lat: 34.6367, lon: 135.4583 },

  // Municipal rapid chargers – smaller cities and rural areas
  { name: '道の駅 なるさわ 急速充電器', operator: '鳴沢村', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '山梨県', lat: 35.4464, lon: 138.6747 },
  { name: '道の駅 白馬 急速充電器', operator: '白馬村', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '長野県', lat: 36.6983, lon: 137.8617 },
  { name: '道の駅 あらい 急速充電器', operator: '妙高市', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: '市町村設置', prefecture: '新潟県', lat: 37.0428, lon: 138.2517 },
  { name: '高山市役所 急速充電器', operator: '高山市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '岐阜県', lat: 36.1461, lon: 137.2525 },
  { name: '金沢駅西口 急速充電器', operator: '金沢市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '石川県', lat: 36.5781, lon: 136.6483 },
  { name: '松江市役所 急速充電器', operator: '松江市', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: '市町村設置', prefecture: '島根県', lat: 35.4681, lon: 133.0486 },
  { name: '高知駅前 急速充電器', operator: '高知市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '高知県', lat: 33.5672, lon: 133.5431 },
  { name: '那覇市役所 急速充電器', operator: '那覇市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '沖縄県', lat: 26.2122, lon: 127.6792 },
  { name: '函館駅前 急速充電器', operator: '函館市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '北海道', lat: 41.7739, lon: 140.7264 },
  { name: '秋田駅前 急速充電器', operator: '秋田市', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: '市町村設置', prefecture: '秋田県', lat: 39.7200, lon: 140.1267 },
  { name: '松山市役所 急速充電器', operator: '松山市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '愛媛県', lat: 33.8389, lon: 132.7658 },
  { name: '長崎駅前 急速充電器', operator: '長崎市', connector_type: 'CHAdeMO', power_kw: 50, num_ports: 1, is_rapid: true, network: '市町村設置', prefecture: '長崎県', lat: 32.7522, lon: 129.8697 },
  { name: '宮崎駅前 急速充電器', operator: '宮崎市', connector_type: 'CHAdeMO', power_kw: 44, num_ports: 1, is_rapid: false, network: '市町村設置', prefecture: '宮崎県', lat: 31.9133, lon: 131.4239 },
];

function generateSeedData() {
  const now = new Date();
  return EV_CHARGERS.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      charger_id: `EVC_${String(i + 1).padStart(4, '0')}`,
      name: c.name,
      operator: c.operator,
      connector_types: [c.connector_type],
      power_kw: c.power_kw,
      num_ports: c.num_ports,
      is_rapid: c.is_rapid,
      is_free: false,
      network: c.network,
      prefecture: c.prefecture,
      status: 'operational',
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'ev_charging_seed',
    },
  }));
}

export default async function collectEvCharging() {
  const [ocm, osm] = await Promise.allSettled([tryOpenChargeMap(), tryOSMChargers()]);
  let features = [];
  let live = false;
  for (const r of [ocm, osm]) {
    if (r.status === 'fulfilled' && Array.isArray(r.value) && r.value.length > 0) {
      features.push(...r.value);
      live = true;
    }
  }
  if (features.length === 0) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: live ? 'ev_charging_live' : 'ev_charging_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'EV charging stations across Japan',
    },
  };
}
