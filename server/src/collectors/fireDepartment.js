/**
 * Fire department dispatch information — fire-department (intel-kind).
 *
 * The Tokyo Fire Department (東京消防庁) publishes a public live
 * "災害情報" (current dispatch) listing at
 *   https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/saigai.html
 * Other municipal fire bureaus run independent, mostly HTML-only pages with
 * no common feed. This collector does a real best-effort fetch of the Tokyo
 * FD dispatch page and extracts the listed active dispatch rows as intel
 * items. No fabrication: on fetch/parse failure or no rows → honest empty.
 *
 * NOTE: the TFD page markup is unverified and may change; the row extraction
 * is defensive and degrades to an empty result rather than guessing.
 */

import { fetchText } from './_liveHelpers.js';
import { intelUid, intelHashKey } from '../utils/intelHelpers.js';

const TFD_URL = 'https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/saigai.html';
const SOURCE_ID = 'fire-department';

function emptyResult(source) {
  return {
    kind: 'intel',
    items: [],
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      source,
      description: 'Tokyo Fire Department current dispatch / disaster information',
    },
  };
}

// Strip tags and collapse whitespace from an HTML fragment.
function textOf(html) {
  return html
    .replace(/<[^>]+>/g, ' ')
    .replace(/&nbsp;/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

export default async function collectFireDepartment() {
  let html;
  try {
    html = await fetchText(TFD_URL, { timeoutMs: 15000 });
  } catch {
    return emptyResult('fire_department_unavailable');
  }
  if (!html || typeof html !== 'string') {
    return emptyResult('fire_department_unavailable');
  }

  const items = [];
  // Dispatch rows are rendered as table rows; extract <tr> cells defensively.
  const trRe = /<tr\b[^>]*>([\s\S]*?)<\/tr>/gi;
  let m;
  while ((m = trRe.exec(html)) !== null) {
    const cells = [];
    const tdRe = /<t[dh]\b[^>]*>([\s\S]*?)<\/t[dh]>/gi;
    let c;
    while ((c = tdRe.exec(m[1])) !== null) {
      const t = textOf(c[1]);
      if (t) cells.push(t);
    }
    if (cells.length < 2) continue;
    // Skip header-ish rows (no digit anywhere — dispatch rows carry a time).
    const joined = cells.join(' / ');
    if (!/\d/.test(joined)) continue;
    if (/災害種別|発生時刻|発生場所/.test(joined) && cells.length <= 3) continue;
    items.push({
      uid: intelUid(SOURCE_ID, intelHashKey(joined)),
      title: cells[0].slice(0, 120),
      summary: joined.slice(0, 280),
      body: joined,
      link: TFD_URL,
      published_at: null,
      tags: ['fire', 'dispatch', 'tokyo'],
      properties: { cells, agency: '東京消防庁' },
    });
  }

  if (items.length === 0) return emptyResult('fire_department_unavailable');

  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source: 'fire_department',
      description: 'Tokyo Fire Department current dispatch / disaster information',
    },
  };
}
