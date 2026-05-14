/**
 * e-Gov national law search API.
 * https://laws.e-gov.go.jp/api/1/lawlists/1
 * Anonymous. XML response — we extract LawName/LawNo per row.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'egov-laws';
const API_URL = 'https://laws.e-gov.go.jp/api/1/lawlists/1';
const TIMEOUT_MS = 10000;
const MAX_ITEMS = 200;

function pickTag(xml, tag) {
  const re = new RegExp(`<${tag}\\b[^>]*>([\\s\\S]*?)<\\/${tag}>`, 'gi');
  const out = [];
  let m;
  while ((m = re.exec(xml)) !== null) out.push(m[1].trim());
  return out;
}

export default async function collectEgovLaws() {
  let items = [];
  let live = false;
  let totalCount = 0;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: ctrl.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const xml = await res.text();
    // Pull LawNameListInfo blocks; each contains LawId/LawName/LawNo/PromulgationDate
    const blocks = (xml.match(/<LawNameListInfo\b[^>]*>[\s\S]*?<\/LawNameListInfo>/g) || []).slice(0, MAX_ITEMS);
    items = blocks.map((b) => {
      const id = pickTag(b, 'LawId')[0] || pickTag(b, 'LawNum')[0];
      const name = pickTag(b, 'LawName')[0];
      const no = pickTag(b, 'LawNo')[0];
      const date = pickTag(b, 'PromulgationDate')[0];
      return {
        uid: intelUid(SOURCE_ID, id, name),
        title: name || id || 'law',
        summary: no || null,
        link: id ? `https://laws.e-gov.go.jp/law/${encodeURIComponent(id)}` : null,
        language: 'ja',
        published_at: date && /^\d{8}$/.test(date)
          ? `${date.slice(0,4)}-${date.slice(4,6)}-${date.slice(6,8)}T00:00:00Z`
          : null,
        tags: ['law', 'e-gov'],
        properties: { law_id: id || null, law_no: no || null },
      };
    });
    totalCount = (xml.match(/<LawName\b/g) || []).length;
    live = items.length > 0;
  } catch { /* fall through */ }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'e-Gov national law & ordinance catalog',
    extraMeta: { total_laws_in_catalog: totalCount },
  });
}
