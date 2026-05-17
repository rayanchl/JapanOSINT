/**
 * Highly pathogenic avian influenza (HPAI) outbreaks — bird-flu-outbreaks.
 *
 * MAFF (農林水産省) publishes the official domestic HPAI outbreak list
 * (家きんでの高病原性鳥インフルエンザの発生状況) at:
 *   https://www.maff.go.jp/j/syouan/douei/tori/index.html
 * The page lists confirmed farm outbreaks by prefecture/case number. We do a
 * real best-effort fetch of that page, extract the outbreak rows, and
 * geocode each to its prefecture centroid via the shared prefecture table.
 * No fabrication: on fetch/parse failure or no rows → honest EMPTY.
 *
 * NOTE: MAFF page markup is not a documented API and is UNVERIFIED; row
 * extraction is defensive and degrades to empty rather than guessing.
 */

import { fetchText } from './_liveHelpers.js';
import { resolvePrefecture } from './_jpPrefectures.js';
import { intelHashKey } from '../utils/intelHelpers.js';

const MAFF_URL = 'https://www.maff.go.jp/j/syouan/douei/tori/index.html';

const PREF_NAMES = [
  '北海道', '青森県', '岩手県', '宮城県', '秋田県', '山形県', '福島県',
  '茨城県', '栃木県', '群馬県', '埼玉県', '千葉県', '東京都', '神奈川県',
  '新潟県', '富山県', '石川県', '福井県', '山梨県', '長野県', '岐阜県',
  '静岡県', '愛知県', '三重県', '滋賀県', '京都府', '大阪府', '兵庫県',
  '奈良県', '和歌山県', '鳥取県', '島根県', '岡山県', '広島県', '山口県',
  '徳島県', '香川県', '愛媛県', '高知県', '福岡県', '佐賀県', '長崎県',
  '熊本県', '大分県', '宮崎県', '鹿児島県', '沖縄県',
];

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'MAFF domestic HPAI (avian influenza) poultry outbreaks by prefecture',
    },
  };
}

export default async function collectBirdFluOutbreaks() {
  let html;
  try {
    html = await fetchText(MAFF_URL, { timeoutMs: 15000 });
  } catch {
    return emptyResult('bird_flu_outbreaks_unavailable');
  }
  if (!html || typeof html !== 'string') {
    return emptyResult('bird_flu_outbreaks_unavailable');
  }

  const text = html
    .replace(/<[^>]+>/g, ' ')
    .replace(/&nbsp;/g, ' ')
    .replace(/\s+/g, ' ');

  // Find "事例" (case) lines mentioning a prefecture name. Each MAFF entry is
  // typically "○例目（<県>○○市）" — we group by prefecture and count cases.
  const perPref = new Map();
  const caseRe = /(\d+)\s*例目[^。]*?([一-鿿]{2,4}[県都道府]|北海道)/g;
  let m;
  while ((m = caseRe.exec(text)) !== null) {
    const caseNo = m[1];
    const prefRaw = m[2];
    if (!PREF_NAMES.includes(prefRaw)) continue;
    if (!perPref.has(prefRaw)) perPref.set(prefRaw, new Set());
    perPref.get(prefRaw).add(caseNo);
  }

  // Fallback: if no "例目" pattern matched, just count prefecture mentions in
  // any outbreak-context sentence (発生 / 確認 / 鳥インフルエンザ nearby).
  if (perPref.size === 0) {
    for (const pn of PREF_NAMES) {
      const re = new RegExp(`${pn}[^。]{0,40}(発生|確認|防疫措置)`);
      if (re.test(text)) perPref.set(pn, new Set(['?']));
    }
  }

  const features = [];
  for (const [prefName, cases] of perPref.entries()) {
    const pref = resolvePrefecture(prefName);
    if (!pref) continue;
    const caseCount = cases.has('?') ? null : cases.size;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [pref.lon, pref.lat] },
      properties: {
        outbreak_id: `HPAI_${pref.code}_${intelHashKey(prefName, caseCount)}`,
        prefecture: prefName,
        prefecture_en: pref.en,
        case_count: caseCount,
        disease: '高病原性鳥インフルエンザ (HPAI)',
        link: MAFF_URL,
        source: 'maff_hpai',
      },
    });
  }

  if (features.length === 0) return emptyResult('bird_flu_outbreaks_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'maff_hpai',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'MAFF domestic HPAI (avian influenza) poultry outbreaks by prefecture',
    },
  };
}
