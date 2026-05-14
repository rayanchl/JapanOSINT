/**
 * Houjin Bangou (法人番号) — Japanese National Tax Agency corporate-number
 * registry. Every JP-registered company has a 13-digit ID; the API exposes
 * recent additions/changes/dissolutions, which is gold for shell-company
 * tracking and address-shift detection.
 *
 * Endpoint (v4 REST, CSV/JSON):
 *   GET https://api.houjin-bangou.nta.go.jp/4/diff
 *   ?id=<APP_ID>&from=YYYY-MM-DD&to=YYYY-MM-DD&type=12
 *   (type=12 = JSON; CSV is type=01)
 *
 * Auth: HOUJIN_BANGOU_KEY — free signup at
 *   https://www.houjin-bangou.nta.go.jp/webapi/
 *
 * Default window: previous 7 days (rolling), to keep payload bounded.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'houjin-bangou';
const BASE = 'https://api.houjin-bangou.nta.go.jp/4/diff';
const TIMEOUT_MS = 20000;

function ymd(d) {
  return d.toISOString().slice(0, 10);
}

export default async function collectHoujinBangou() {
  const id = process.env.HOUJIN_BANGOU_KEY;
  if (!id) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'JP corporate-number registry diffs',
      extraMeta: { env_hint: 'Set HOUJIN_BANGOU_KEY (free signup: https://www.houjin-bangou.nta.go.jp/webapi/)' },
    });
  }

  const days = parseInt(process.env.HOUJIN_BANGOU_DAYS || '7', 10);
  const to = new Date();
  const from = new Date(Date.now() - days * 86400000);
  const params = new URLSearchParams({
    id, from: ymd(from), to: ymd(to), type: '12', history: '0',
  });

  let items = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const corps = Array.isArray(json.corporations) ? json.corporations
      : Array.isArray(json) ? json
      : Array.isArray(json.data) ? json.data : [];
    const PROC = { '01': 'new', '11': 'name-change', '21': 'address-change', '71': 'closed' };
    items = corps.slice(0, 500).map((c) => {
      const cn = c.corporateNumber || c.corporate_number;
      const proc = c.process || null;
      return {
        uid: intelUid(SOURCE_ID, cn, c.changeDate || c.updateDate),
        title: c.name || `Corporate #${cn}`,
        summary: [PROC[proc] || proc, c.prefectureName || c.prefecture_name, c.cityName || c.city_name]
          .filter(Boolean).join(' · ') || null,
        language: 'ja',
        published_at: c.updateDate || c.update_date || c.changeDate || c.change_date || null,
        tags: ['corporate-registry', proc ? `process:${PROC[proc] || proc}` : null].filter(Boolean),
        properties: {
          corporate_number: cn || null,
          process: proc,
          name_kana: c.furigana || null,
          name_en: c.nameEn || c.name_en || null,
          kind: c.kind || null,
          prefecture_name: c.prefectureName || c.prefecture_name || null,
          city_name: c.cityName || c.city_name || null,
          street_number: c.streetNumber || c.street_number || null,
          post_code: c.postCode || c.post_code || null,
          close_date: c.closeDate || c.close_date || null,
          close_cause: c.closeCause || c.close_cause || null,
          successor_corp: c.successorCorporateNumber || c.successor_corporate_number || null,
        },
      };
    });
    live = items.length > 0;
  } catch (err) {
    console.warn('[houjinBangou] fetch failed:', err?.message);
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'Houjin Bangou diff API — recent JP corporate adds/changes/closes',
    extraMeta: { window_from: ymd(from), window_to: ymd(to) },
  });
}
