/**
 * Social Media Collector
 * Synthetic geotagged social media activity across Japanese cities
 * Clustered around popular areas
 */

import { fetchJson } from './_liveHelpers.js';

async function tryLive() {
  const data = await fetchJson('https://api.wikipedia.org/core/v1/wikipedia/en/search/page?q=Japan&limit=20');
  if (!data || !Array.isArray(data.pages) || data.pages.length === 0) return null;
  // Tokyo center for points without coordinates (Wikipedia search results lack geo)
  const tokyoLat = 35.6812;
  const tokyoLon = 139.7671;
  return data.pages.map((page, i) => ({
    type: 'Feature',
    geometry: {
      type: 'Point',
      coordinates: [
        tokyoLon + ((i % 5) - 2) * 0.01,
        tokyoLat + (Math.floor(i / 5) - 2) * 0.01,
      ],
    },
    properties: {
      post_id: `SM_LIVE_${String(i + 1).padStart(4, '0')}`,
      platform: 'wikipedia',
      content_type: 'article',
      sentiment: 'neutral',
      engagement_count: 0,
      hashtags: [],
      area_name: page.title || `Page ${page.id}`,
      description: page.description || page.excerpt || '',
      url: page.key ? `https://en.wikipedia.org/wiki/${page.key}` : null,
      timestamp: new Date().toISOString(),
      has_location: false,
      source: 'wikipedia_live',
    },
  }));
}

const PLATFORMS = ['twitter', 'instagram', 'flickr'];
const CONTENT_TYPES = ['photo', 'text', 'video'];
const SENTIMENTS = ['positive', 'neutral', 'negative'];

// Popular areas with clustering weights
const HOTSPOTS = [
  // Tokyo - heavy clustering
  { area: '渋谷スクランブル交差点', lat: 35.6595, lon: 139.7004, weight: 8 },
  { area: '新宿歌舞伎町', lat: 35.6938, lon: 139.7036, weight: 6 },
  { area: '原宿竹下通り', lat: 35.6702, lon: 139.7035, weight: 7 },
  { area: '秋葉原電気街', lat: 35.6984, lon: 139.7731, weight: 5 },
  { area: '浅草雷門', lat: 35.7114, lon: 139.7966, weight: 6 },
  { area: '東京スカイツリー', lat: 35.7101, lon: 139.8107, weight: 5 },
  { area: '東京タワー', lat: 35.6586, lon: 139.7454, weight: 4 },
  { area: '銀座中央通り', lat: 35.6717, lon: 139.7637, weight: 4 },
  { area: 'お台場', lat: 35.6267, lon: 139.7752, weight: 4 },
  { area: '池袋サンシャイン', lat: 35.7295, lon: 139.7182, weight: 4 },
  { area: '六本木ヒルズ', lat: 35.6605, lon: 139.7292, weight: 3 },
  { area: '代官山', lat: 35.6490, lon: 139.7021, weight: 3 },
  { area: '上野公園', lat: 35.7146, lon: 139.7732, weight: 3 },
  { area: '豊洲市場', lat: 35.6460, lon: 139.7849, weight: 3 },
  // Osaka
  { area: '道頓堀', lat: 34.6687, lon: 135.5013, weight: 7 },
  { area: '心斎橋', lat: 34.6748, lon: 135.5012, weight: 5 },
  { area: '通天閣', lat: 34.6522, lon: 135.5062, weight: 4 },
  { area: '大阪城', lat: 34.6873, lon: 135.5259, weight: 5 },
  { area: 'ユニバーサルスタジオ', lat: 34.6654, lon: 135.4323, weight: 6 },
  // Kyoto
  { area: '伏見稲荷大社', lat: 35.0326, lon: 135.7727, weight: 6 },
  { area: '金閣寺', lat: 35.0394, lon: 135.7292, weight: 5 },
  { area: '嵐山竹林', lat: 35.0170, lon: 135.6713, weight: 5 },
  { area: '清水寺', lat: 34.9949, lon: 135.7850, weight: 5 },
  // Other
  { area: '富士山', lat: 35.3606, lon: 138.7274, weight: 4 },
  { area: '宮島', lat: 34.2960, lon: 132.3196, weight: 3 },
  { area: '奈良公園', lat: 34.6851, lon: 135.8430, weight: 4 },
  { area: '鎌倉大仏', lat: 35.3167, lon: 139.5358, weight: 3 },
  { area: '札幌時計台', lat: 43.0625, lon: 141.3536, weight: 2 },
  { area: '国際通り', lat: 26.3358, lon: 127.6862, weight: 3 },
  { area: '博多中洲', lat: 33.5920, lon: 130.4080, weight: 3 },
  { area: '横浜中華街', lat: 35.4429, lon: 139.6469, weight: 3 },
  { area: '函館山', lat: 41.7589, lon: 140.7022, weight: 2 },
];

const SAMPLE_TAGS = [
  '#japan', '#tokyo', '#osaka', '#kyoto', '#travel', '#日本',
  '#東京', '#大阪', '#京都', '#旅行', '#グルメ', '#写真',
  '#photography', '#food', '#sakura', '#桜', '#temple',
  '#shrine', '#neon', '#nightlife', '#ramen', '#ラーメン',
];

function seededRandom(seed) {
  let x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  const now = new Date();
  const TARGET = 100;
  let idx = 0;

  const totalWeight = HOTSPOTS.reduce((s, h) => s + h.weight, 0);

  for (const spot of HOTSPOTS) {
    const count = Math.max(1, Math.round((spot.weight / totalWeight) * TARGET));

    for (let j = 0; j < count && features.length < TARGET; j++) {
      idx++;
      const r1 = seededRandom(idx);
      const r2 = seededRandom(idx * 7);
      const r3 = seededRandom(idx * 13);
      const r4 = seededRandom(idx * 19);
      const r5 = seededRandom(idx * 29);

      const lat = spot.lat + (r1 - 0.5) * 0.006;
      const lon = spot.lon + (r2 - 0.5) * 0.008;

      const platform = PLATFORMS[Math.floor(r3 * PLATFORMS.length)];
      const contentType = CONTENT_TYPES[Math.floor(r4 * CONTENT_TYPES.length)];

      // Sentiment weighted positive for tourist spots
      const sentIdx = r5 < 0.55 ? 0 : r5 < 0.85 ? 1 : 2;
      const sentiment = SENTIMENTS[sentIdx];

      // Random timestamp within last 48 hours
      const hoursAgo = Math.floor(seededRandom(idx * 37) * 48);
      const postDate = new Date(now);
      postDate.setHours(postDate.getHours() - hoursAgo);

      const engagement = Math.floor(seededRandom(idx * 43) * 500) +
        (platform === 'instagram' ? 50 : 10);

      const tagCount = 1 + Math.floor(seededRandom(idx * 59) * 4);
      const tags = [];
      for (let t = 0; t < tagCount; t++) {
        const tagIdx = Math.floor(seededRandom(idx * 67 + t) * SAMPLE_TAGS.length);
        if (!tags.includes(SAMPLE_TAGS[tagIdx])) tags.push(SAMPLE_TAGS[tagIdx]);
      }

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          post_id: `SM_${String(idx).padStart(4, '0')}`,
          platform,
          content_type: contentType,
          sentiment,
          engagement_count: engagement,
          hashtags: tags,
          area_name: spot.area,
          timestamp: postDate.toISOString(),
          has_location: true,
          source: 'social_seed',
        },
      });
    }
  }

  return features.slice(0, TARGET);
}

export default async function collectSocialMedia() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'social_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Synthetic geotagged social media activity across Japanese cities',
    },
    metadata: {},
  };
}
