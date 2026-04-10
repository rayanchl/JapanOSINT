/**
 * UNESCO World Heritage Sites Collector
 * UNESCO World Heritage Centre — all 25 Japanese sites (cultural + natural).
 */

import { fetchText } from './_liveHelpers.js';

async function tryLive() {
  const xml = await fetchText('https://whc.unesco.org/en/list/xml/');
  if (!xml || xml.length < 100) return null;
  const rows = xml.split(/<row[>\s]/i).slice(1);
  const features = [];
  for (const row of rows) {
    const iso = (row.match(/<iso_code>([^<]*)<\/iso_code>/i) || [])[1]?.toLowerCase();
    if (!iso || !iso.includes('jp')) continue;
    const name = (row.match(/<site>([^<]*)<\/site>/i) || row.match(/<name_en>([^<]*)<\/name_en>/i) || [])[1];
    const lon = parseFloat((row.match(/<longitude>([^<]*)<\/longitude>/i) || [])[1]);
    const lat = parseFloat((row.match(/<latitude>([^<]*)<\/latitude>/i) || [])[1]);
    const category = (row.match(/<category>([^<]*)<\/category>/i) || [])[1];
    const date = (row.match(/<date_inscribed>([^<]*)<\/date_inscribed>/i) || [])[1];
    if (isNaN(lat) || isNaN(lon) || !name) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        whs_id: `WHS_LIVE_${String(features.length + 1).padStart(5, '0')}`,
        name,
        name_en: name,
        kind: (category || '').toLowerCase() || 'cultural',
        year: parseInt(date, 10) || null,
        country: 'JP',
        source: 'unesco_whs_live',
      },
    });
  }
  return features.length > 0 ? features : null;
}

const SEED_WHS = [
  // Cultural
  { name: '法隆寺地域の仏教建造物', name_en: 'Buddhist Monuments in the Hōryū-ji Area', lat: 34.6144, lon: 135.7344, kind: 'cultural', year: 1993 },
  { name: '姫路城', name_en: 'Himeji-jō', lat: 34.8394, lon: 134.6939, kind: 'cultural', year: 1993 },
  { name: '古都京都の文化財', name_en: 'Historic Monuments of Ancient Kyoto', lat: 35.0394, lon: 135.7292, kind: 'cultural', year: 1994 },
  { name: '白川郷・五箇山の合掌造り集落', name_en: 'Historic Villages of Shirakawa-gō and Gokayama', lat: 36.2583, lon: 136.9061, kind: 'cultural', year: 1995 },
  { name: '原爆ドーム', name_en: 'Hiroshima Peace Memorial', lat: 34.3955, lon: 132.4536, kind: 'cultural', year: 1996 },
  { name: '厳島神社', name_en: 'Itsukushima Shinto Shrine', lat: 34.2958, lon: 132.3197, kind: 'cultural', year: 1996 },
  { name: '古都奈良の文化財', name_en: 'Historic Monuments of Ancient Nara', lat: 34.6850, lon: 135.8050, kind: 'cultural', year: 1998 },
  { name: '日光の社寺', name_en: 'Shrines and Temples of Nikkō', lat: 36.7581, lon: 139.5986, kind: 'cultural', year: 1999 },
  { name: '琉球王国のグスク及び関連遺産群', name_en: 'Gusuku Sites of Ryukyu', lat: 26.3361, lon: 127.7414, kind: 'cultural', year: 2000 },
  { name: '紀伊山地の霊場と参詣道', name_en: 'Sacred Sites of the Kii Mountain Range', lat: 34.1667, lon: 135.7833, kind: 'cultural', year: 2004 },
  { name: '石見銀山遺跡とその文化的景観', name_en: 'Iwami Ginzan Silver Mine', lat: 35.1083, lon: 132.4358, kind: 'cultural', year: 2007 },
  { name: '平泉 — 仏国土を表す建築・庭園', name_en: 'Hiraizumi', lat: 38.9883, lon: 141.1131, kind: 'cultural', year: 2011 },
  { name: '富士山 — 信仰の対象と芸術の源泉', name_en: 'Fujisan, Sacred Place', lat: 35.3606, lon: 138.7311, kind: 'cultural', year: 2013 },
  { name: '富岡製糸場と絹産業遺産群', name_en: 'Tomioka Silk Mill', lat: 36.2553, lon: 138.8881, kind: 'cultural', year: 2014 },
  { name: '明治日本の産業革命遺産', name_en: 'Sites of Japan\u2019s Meiji Industrial Revolution', lat: 32.6275, lon: 129.7361, kind: 'cultural', year: 2015 },
  { name: '国立西洋美術館 (ル・コルビュジエ)', name_en: 'National Museum of Western Art', lat: 35.7156, lon: 139.7758, kind: 'cultural', year: 2016 },
  { name: '宗像・沖ノ島と関連遺産群', name_en: 'Sacred Island of Okinoshima', lat: 34.2417, lon: 130.1056, kind: 'cultural', year: 2017 },
  { name: '長崎と天草地方の潜伏キリシタン関連遺産', name_en: 'Hidden Christian Sites in Nagasaki', lat: 32.7500, lon: 129.8833, kind: 'cultural', year: 2018 },
  { name: '百舌鳥・古市古墳群', name_en: 'Mozu-Furuichi Kofun Group', lat: 34.5639, lon: 135.4881, kind: 'cultural', year: 2019 },
  { name: '北海道・北東北の縄文遺跡群', name_en: 'Jōmon Prehistoric Sites', lat: 40.8225, lon: 140.6783, kind: 'cultural', year: 2021 },
  { name: '佐渡島の金山', name_en: 'Sado Island Gold Mines', lat: 38.0419, lon: 138.2453, kind: 'cultural', year: 2024 },
  // Natural
  { name: '屋久島', name_en: 'Yakushima', lat: 30.3500, lon: 130.5167, kind: 'natural', year: 1993 },
  { name: '白神山地', name_en: 'Shirakami-Sanchi', lat: 40.4833, lon: 140.1500, kind: 'natural', year: 1993 },
  { name: '知床', name_en: 'Shiretoko', lat: 44.0667, lon: 145.0000, kind: 'natural', year: 2005 },
  { name: '小笠原諸島', name_en: 'Ogasawara Islands', lat: 27.0833, lon: 142.1833, kind: 'natural', year: 2011 },
  { name: '奄美大島・徳之島・沖縄島北部・西表島', name_en: 'Amami-Ōshima, Tokunoshima, Okinawa, Iriomote', lat: 28.3000, lon: 129.5000, kind: 'natural', year: 2021 },
];

function generateSeedData() {
  return SEED_WHS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      whs_id: `WHS_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      name_en: s.name_en,
      kind: s.kind,
      year: s.year,
      country: 'JP',
      source: 'unesco_whs_seed',
    },
  }));
}

export default async function collectUnescoHeritage() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'unesco_heritage',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'UNESCO World Heritage Sites in Japan: cultural (21) + natural (5)',
    },
    metadata: {},
  };
}
