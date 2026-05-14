/**
 * Suumo — rental listing density per prefecture.
 *
 * Suumo's rental search at https://suumo.jp/chintai/ encodes prefecture
 * in the URL (`/chintai/{prefecture-code}/`) and each search page shows
 * a total count. We fetch just the first page for each prefecture and
 * parse the listing total, pinning the result at the prefecture
 * centroid.
 *
 * LEGAL NOTE: Suumo's ToS is Japan-standard — casual light polling has
 * not been known to trigger blocks, but this is a scrape. Respect
 * robots.txt and keep the cadence >= 24h.
 *
 * No auth. Uses prefecture centroids pulled from the existing helper.
 */

const TIMEOUT_MS = 15000;

// Rough centroids of each prefecture. Used to pin the aggregate count
// somewhere sensible for map rendering.
const PREFECTURE_CENTROID = {
  '北海道': { lat: 43.0642, lon: 141.3469 }, '青森県': { lat: 40.8243, lon: 140.7400 },
  '岩手県': { lat: 39.7036, lon: 141.1527 }, '宮城県': { lat: 38.2688, lon: 140.8721 },
  '秋田県': { lat: 39.7186, lon: 140.1024 }, '山形県': { lat: 38.2404, lon: 140.3634 },
  '福島県': { lat: 37.7503, lon: 140.4676 }, '茨城県': { lat: 36.3418, lon: 140.4468 },
  '栃木県': { lat: 36.5657, lon: 139.8836 }, '群馬県': { lat: 36.3912, lon: 139.0608 },
  '埼玉県': { lat: 35.8570, lon: 139.6489 }, '千葉県': { lat: 35.6047, lon: 140.1233 },
  '東京都': { lat: 35.6895, lon: 139.6917 }, '神奈川県': { lat: 35.4478, lon: 139.6425 },
  '新潟県': { lat: 37.9161, lon: 139.0364 }, '富山県': { lat: 36.6953, lon: 137.2113 },
  '石川県': { lat: 36.5946, lon: 136.6256 }, '福井県': { lat: 36.0652, lon: 136.2216 },
  '山梨県': { lat: 35.6640, lon: 138.5683 }, '長野県': { lat: 36.6513, lon: 138.1811 },
  '岐阜県': { lat: 35.3912, lon: 136.7223 }, '静岡県': { lat: 34.9770, lon: 138.3830 },
  '愛知県': { lat: 35.1802, lon: 136.9066 }, '三重県': { lat: 34.7303, lon: 136.5086 },
  '滋賀県': { lat: 35.0045, lon: 135.8686 }, '京都府': { lat: 35.0212, lon: 135.7556 },
  '大阪府': { lat: 34.6863, lon: 135.5200 }, '兵庫県': { lat: 34.6913, lon: 135.1830 },
  '奈良県': { lat: 34.6851, lon: 135.8050 }, '和歌山県': { lat: 34.2261, lon: 135.1675 },
  '鳥取県': { lat: 35.5036, lon: 134.2378 }, '島根県': { lat: 35.4723, lon: 133.0505 },
  '岡山県': { lat: 34.6618, lon: 133.9350 }, '広島県': { lat: 34.3963, lon: 132.4596 },
  '山口県': { lat: 34.1859, lon: 131.4707 }, '徳島県': { lat: 34.0657, lon: 134.5592 },
  '香川県': { lat: 34.3401, lon: 134.0434 }, '愛媛県': { lat: 33.8416, lon: 132.7657 },
  '高知県': { lat: 33.5597, lon: 133.5311 }, '福岡県': { lat: 33.6064, lon: 130.4181 },
  '佐賀県': { lat: 33.2494, lon: 130.2988 }, '長崎県': { lat: 32.7503, lon: 129.8779 },
  '熊本県': { lat: 32.7898, lon: 130.7417 }, '大分県': { lat: 33.2382, lon: 131.6126 },
  '宮崎県': { lat: 31.9111, lon: 131.4239 }, '鹿児島県': { lat: 31.5602, lon: 130.5581 },
  '沖縄県': { lat: 26.2124, lon: 127.6809 },
};

// Suumo prefecture slugs. These are the two-letter kanji-romaji codes
// Suumo uses in the URL path (e.g. `/chintai/tokyo/`).
const SUUMO_SLUGS = {
  '北海道': 'hokkaido', '青森県': 'aomori', '岩手県': 'iwate', '宮城県': 'miyagi',
  '秋田県': 'akita', '山形県': 'yamagata', '福島県': 'fukushima',
  '茨城県': 'ibaraki', '栃木県': 'tochigi', '群馬県': 'gumma', '埼玉県': 'saitama',
  '千葉県': 'chiba', '東京都': 'tokyo', '神奈川県': 'kanagawa',
  '新潟県': 'nigata', '富山県': 'toyama', '石川県': 'ishikawa', '福井県': 'fukui',
  '山梨県': 'yamanashi', '長野県': 'nagano', '岐阜県': 'gifu', '静岡県': 'shizuoka',
  '愛知県': 'aichi', '三重県': 'mie',
  '滋賀県': 'shiga', '京都府': 'kyoto', '大阪府': 'osaka', '兵庫県': 'hyogo',
  '奈良県': 'nara', '和歌山県': 'wakayama',
  '鳥取県': 'tottori', '島根県': 'shimane', '岡山県': 'okayama', '広島県': 'hiroshima', '山口県': 'yamaguchi',
  '徳島県': 'tokushima', '香川県': 'kagawa', '愛媛県': 'ehime', '高知県': 'kochi',
  '福岡県': 'fukuoka', '佐賀県': 'saga', '長崎県': 'nagasaki', '熊本県': 'kumamoto',
  '大分県': 'oita', '宮崎県': 'miyazaki', '鹿児島県': 'kagoshima', '沖縄県': 'okinawa',
};

async function fetchCountForSlug(slug) {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`https://suumo.jp/chintai/${slug}/`, {
      signal: controller.signal,
      headers: {
        'user-agent': 'Mozilla/5.0 (compatible; JapanOSINT/1.0)',
        'accept': 'text/html',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const html = await res.text();
    // Suumo prints totals like "物件数 123,456 件" or "該当件数 123,456件".
    const m = html.match(/([0-9,]{2,})\s*件/);
    if (!m) return null;
    const n = parseInt(m[1].replace(/,/g, ''), 10);
    return Number.isFinite(n) ? n : null;
  } catch {
    return null;
  }
}

export default async function collectSuumoRentalDensity() {
  const entries = Object.entries(SUUMO_SLUGS);
  const features = [];
  let liveHits = 0;

  for (const [prefJa, slug] of entries) {
    const centroid = PREFECTURE_CENTROID[prefJa];
    if (!centroid) continue;
    // Sequential polling with a tiny gap to stay under Suumo's radar.
    const count = await fetchCountForSlug(slug);
    if (count != null) liveHits++;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [centroid.lon, centroid.lat] },
      properties: {
        id: `SUUMO_${slug}`,
        prefecture_ja: prefJa,
        prefecture_slug: slug,
        rental_listings: count,
        source: count != null ? 'suumo_live' : 'suumo_unavailable',
      },
    });
    await new Promise((r) => setTimeout(r, 250));
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveHits > 0 ? 'suumo_scrape' : 'suumo_offline',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live_hits: liveHits,
      cadence_hint: 'Run at most once per 24 h — respect Suumo robots.txt',
      description: 'Suumo rental-listing counts per prefecture (density proxy)',
    },
  };
}
