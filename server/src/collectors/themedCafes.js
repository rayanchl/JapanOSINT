/**
 * Themed Cafes Collector
 * Japan is famous for animal/character/maid themed cafes — cat, owl,
 * hedgehog, capybara, maid, butler, anime character, robot cafes.
 * Live: OSM Overpass `amenity=cafe animal=yes|cafe:type=maid` (rare
 * tagging, so this typically falls through to seed).
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="cafe"]["cafe:type"](area.jp);node["amenity"="cafe"]["animal"](area.jp);node["amenity"="cafe"]["name"~"メイド|cat cafe|owl cafe|maid"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        cafe_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Themed Cafe ${i + 1}`,
        name_ja: el.tags?.name || null,
        theme: el.tags?.['cafe:type'] || el.tags?.animal || 'unknown',
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const SEED_CAFES = [
  // Maid cafes — Akihabara flagship district
  { name: '@home cafe Akihabara', lat: 35.7019, lon: 139.7714, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: '@home cafe' },
  { name: '@home cafe Akiba Donkihote', lat: 35.7000, lon: 139.7719, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: '@home cafe' },
  { name: 'Maidreamin Akihabara Denki', lat: 35.7014, lon: 139.7731, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Maidreamin Akihabara Central', lat: 35.7003, lon: 139.7725, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Maidreamin Shibuya', lat: 35.6597, lon: 139.7002, prefecture: '東京都', city: '渋谷区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Maidreamin Shinjuku', lat: 35.6925, lon: 139.7028, prefecture: '東京都', city: '新宿区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Maidreamin Umeda Osaka', lat: 34.7025, lon: 135.4981, prefecture: '大阪府', city: '大阪市北区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Maidreamin Namba', lat: 34.6667, lon: 135.5011, prefecture: '大阪府', city: '大阪市中央区', theme: 'maid', chain: 'Maidreamin' },
  { name: 'Schatzkiste Akihabara', lat: 35.7025, lon: 139.7719, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: 'Schatzkiste' },
  { name: 'Pinafore Akihabara', lat: 35.7011, lon: 139.7728, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: 'Pinafore' },
  { name: 'Cure Maid Cafe Akihabara', lat: 35.7014, lon: 139.7728, prefecture: '東京都', city: '千代田区', theme: 'maid', chain: 'Cure Maid Cafe' },

  // Cat cafes
  { name: 'Cat Cafe Calico Shinjuku', lat: 35.6944, lon: 139.7031, prefecture: '東京都', city: '新宿区', theme: 'cat', chain: 'Calico' },
  { name: 'Cat Cafe Mocha Harajuku', lat: 35.6719, lon: 139.7047, prefecture: '東京都', city: '渋谷区', theme: 'cat', chain: 'Mocha' },
  { name: 'Cat Cafe Mocha Shibuya', lat: 35.6597, lon: 139.6983, prefecture: '東京都', city: '渋谷区', theme: 'cat', chain: 'Mocha' },
  { name: 'Cat Cafe Mocha Ikebukuro', lat: 35.7294, lon: 139.7117, prefecture: '東京都', city: '豊島区', theme: 'cat', chain: 'Mocha' },
  { name: 'Cat Cafe Mocha Umeda', lat: 34.7028, lon: 135.4975, prefecture: '大阪府', city: '大阪市北区', theme: 'cat', chain: 'Mocha' },
  { name: 'Temari no Ouchi Kichijoji', lat: 35.7028, lon: 139.5794, prefecture: '東京都', city: '武蔵野市', theme: 'cat', chain: 'Temari no Ouchi' },
  { name: 'Nyanda Kichijoji', lat: 35.7033, lon: 139.5808, prefecture: '東京都', city: '武蔵野市', theme: 'cat', chain: 'Nyanda' },
  { name: 'Cat Cafe Cateriam Namba', lat: 34.6661, lon: 135.5022, prefecture: '大阪府', city: '大阪市中央区', theme: 'cat', chain: 'Cateriam' },
  { name: 'Cat Cafe Nyafe Melange Yoyogi', lat: 35.6831, lon: 139.7019, prefecture: '東京都', city: '渋谷区', theme: 'cat', chain: 'Nyafe Melange' },

  // Owl cafes
  { name: 'Akiba Fukurou Akihabara', lat: 35.6992, lon: 139.7747, prefecture: '東京都', city: '千代田区', theme: 'owl', chain: 'Akiba Fukurou' },
  { name: 'Fukurou no Mise Tsukishima', lat: 35.6653, lon: 139.7803, prefecture: '東京都', city: '中央区', theme: 'owl', chain: 'Fukurou no Mise' },
  { name: 'Owl Village Harajuku', lat: 35.6708, lon: 139.7031, prefecture: '東京都', city: '渋谷区', theme: 'owl', chain: 'Owl Village' },
  { name: 'Owl Cafe Lucky Osaka', lat: 34.6664, lon: 135.5017, prefecture: '大阪府', city: '大阪市中央区', theme: 'owl', chain: 'Lucky' },
  { name: 'Crew Fukurou Kyoto', lat: 35.0033, lon: 135.7683, prefecture: '京都府', city: '京都市中京区', theme: 'owl', chain: 'Crew Fukurou' },

  // Hedgehog cafes
  { name: 'Harry Hedgehog Cafe Roppongi', lat: 35.6626, lon: 139.7311, prefecture: '東京都', city: '港区', theme: 'hedgehog', chain: 'Harry' },
  { name: 'Harry Hedgehog Cafe Harajuku', lat: 35.6717, lon: 139.7042, prefecture: '東京都', city: '渋谷区', theme: 'hedgehog', chain: 'Harry' },
  { name: 'Harry Hedgehog Cafe Ueno', lat: 35.7122, lon: 139.7769, prefecture: '東京都', city: '台東区', theme: 'hedgehog', chain: 'Harry' },
  { name: 'Chikuchiku Cafe Machida', lat: 35.5417, lon: 139.4467, prefecture: '東京都', city: '町田市', theme: 'hedgehog', chain: 'Chikuchiku' },
  { name: 'Harinezumi Cafe Hedgehog Home Shibuya', lat: 35.6597, lon: 139.7014, prefecture: '東京都', city: '渋谷区', theme: 'hedgehog', chain: 'Hedgehog Home' },

  // Rabbit cafes
  { name: 'Ra.a.g.f Harajuku', lat: 35.6713, lon: 139.7044, prefecture: '東京都', city: '渋谷区', theme: 'rabbit', chain: 'Ra.a.g.f' },
  { name: 'Moff Animal Cafe Shibuya', lat: 35.6594, lon: 139.6989, prefecture: '東京都', city: '渋谷区', theme: 'rabbit', chain: 'Moff Animal' },
  { name: 'Usagi Cafe Omotesando', lat: 35.6672, lon: 139.7117, prefecture: '東京都', city: '渋谷区', theme: 'rabbit', chain: 'Usagi' },

  // Capybara / exotic
  { name: 'Capybara Land Kichijoji', lat: 35.7036, lon: 139.5797, prefecture: '東京都', city: '武蔵野市', theme: 'capybara', chain: 'Capybara Land' },
  { name: 'Animal Cafe Sunshine Ikebukuro', lat: 35.7297, lon: 139.7114, prefecture: '東京都', city: '豊島区', theme: 'exotic', chain: 'Sunshine' },

  // Dog cafes
  { name: 'Dog Heart Harajuku', lat: 35.6719, lon: 139.7036, prefecture: '東京都', city: '渋谷区', theme: 'dog', chain: 'Dog Heart' },
  { name: 'Mocha Dog Cafe Shibuya', lat: 35.6600, lon: 139.6989, prefecture: '東京都', city: '渋谷区', theme: 'dog', chain: 'Mocha Dog' },

  // Reptile / bug cafes
  { name: 'Reptile Cafe Yokohama', lat: 35.4656, lon: 139.6222, prefecture: '神奈川県', city: '横浜市西区', theme: 'reptile', chain: 'Reptile Cafe' },
  { name: 'Beetle Cafe Shibuya', lat: 35.6600, lon: 139.6994, prefecture: '東京都', city: '渋谷区', theme: 'beetle', chain: 'Beetle Cafe' },

  // Character / anime / media
  { name: 'Pokemon Cafe Tokyo Nihonbashi', lat: 35.6825, lon: 139.7744, prefecture: '東京都', city: '中央区', theme: 'character', chain: 'Pokemon Cafe' },
  { name: 'Pokemon Cafe Osaka', lat: 34.7031, lon: 135.4967, prefecture: '大阪府', city: '大阪市北区', theme: 'character', chain: 'Pokemon Cafe' },
  { name: 'Kirby Cafe Tokyo Skytree', lat: 35.7106, lon: 139.8108, prefecture: '東京都', city: '墨田区', theme: 'character', chain: 'Kirby Cafe' },
  { name: 'Kirby Cafe Hakata', lat: 33.5897, lon: 130.4200, prefecture: '福岡県', city: '福岡市博多区', theme: 'character', chain: 'Kirby Cafe' },
  { name: 'Sanrio Cafe Ikebukuro', lat: 35.7294, lon: 139.7103, prefecture: '東京都', city: '豊島区', theme: 'character', chain: 'Sanrio' },
  { name: 'Final Fantasy Eorzea Cafe Akihabara', lat: 35.7006, lon: 139.7725, prefecture: '東京都', city: '千代田区', theme: 'gaming', chain: 'Eorzea Cafe' },
  { name: 'Capcom Cafe Shinjuku', lat: 35.6925, lon: 139.7008, prefecture: '東京都', city: '新宿区', theme: 'gaming', chain: 'Capcom Cafe' },
  { name: 'Gundam Cafe Akihabara', lat: 35.6986, lon: 139.7739, prefecture: '東京都', city: '千代田区', theme: 'anime', chain: 'Gundam Cafe' },
  { name: 'Studio Ghibli Robot Cafe Mitaka', lat: 35.6961, lon: 139.5700, prefecture: '東京都', city: '三鷹市', theme: 'anime', chain: 'Ghibli' },
  { name: 'Evangelion Store Tokyo Harajuku', lat: 35.6708, lon: 139.7028, prefecture: '東京都', city: '渋谷区', theme: 'anime', chain: 'Evangelion Store' },

  // Robot / concept
  { name: 'Robot Restaurant Shinjuku Kabukicho', lat: 35.6956, lon: 139.7014, prefecture: '東京都', city: '新宿区', theme: 'robot', chain: 'Robot Restaurant' },
  { name: 'Pepper Parlor Shibuya Scramble Square', lat: 35.6583, lon: 139.7025, prefecture: '東京都', city: '渋谷区', theme: 'robot', chain: 'Pepper Parlor' },
  { name: 'Henn na Cafe Ginza', lat: 35.6717, lon: 139.7642, prefecture: '東京都', city: '中央区', theme: 'robot', chain: 'Henn na Cafe' },

  // Butler / reverse-maid
  { name: 'Swallowtail Ikebukuro', lat: 35.7283, lon: 139.7114, prefecture: '東京都', city: '豊島区', theme: 'butler', chain: 'Swallowtail' },

  // Prison / hospital / ninja themed
  { name: 'Alcatraz ER Shibuya', lat: 35.6594, lon: 139.6997, prefecture: '東京都', city: '渋谷区', theme: 'concept', chain: 'Alcatraz ER' },
  { name: 'Ninja Akasaka', lat: 35.6747, lon: 139.7367, prefecture: '東京都', city: '港区', theme: 'concept', chain: 'Ninja Akasaka' },

  // Vampire
  { name: 'Vampire Cafe Ginza', lat: 35.6717, lon: 139.7639, prefecture: '東京都', city: '中央区', theme: 'concept', chain: 'Vampire Cafe' },
];

function generateSeedData() {
  return SEED_CAFES.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      cafe_id: `THEMED_${i + 1}`,
      name: c.name,
      theme: c.theme,
      chain: c.chain,
      prefecture: c.prefecture,
      city: c.city,
      country: 'JP',
      source: 'curated_seed',
    },
  }));
}

export default async function collectThemedCafes() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'themed-cafes',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'curated_seed',
      description: 'Japanese themed cafes — maid, cat, owl, hedgehog, character, robot, butler',
    },
    metadata: {},
  };
}
