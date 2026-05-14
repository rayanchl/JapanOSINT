/**
 * FSA EDINET corporate filings (securities disclosures).
 * https://api.edinet-fsa.go.jp/api/v2/documents.json
 * Requires Subscription-Key header. Without a key we emit a tiny seed batch.
 *
 * Non-spatial — emits one intel item per filing.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { getEnv } from '../utils/credentials.js';

const SOURCE_ID = 'edinet-filings';
const TIMEOUT_MS = 10000;
const edinetKey = () => getEnv(null, 'EDINET_API_KEY');

function today() {
  return new Date().toISOString().slice(0, 10);
}

export default async function collectEdinetFilings() {
  let items = [];
  let live = false;

  const API_KEY = edinetKey();
  if (API_KEY) {
    try {
      const ctrl = new AbortController();
      const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
      const url = `https://api.edinet-fsa.go.jp/api/v2/documents.json?date=${today()}&type=2&Subscription-Key=${encodeURIComponent(API_KEY)}`;
      const res = await fetch(url, { signal: ctrl.signal });
      clearTimeout(timer);
      if (res.ok) {
        const data = await res.json();
        const list = data?.results ?? [];
        items = list.slice(0, 200).map((d, i) => ({
          uid: intelUid(SOURCE_ID, d.docID, `idx_${i}`),
          title: d.filerName ? `${d.filerName} — ${d.docDescription || d.formCode || 'filing'}` : (d.docDescription || `Filing ${i}`),
          summary: d.docDescription || null,
          author: d.filerName || null,
          language: 'ja',
          published_at: d.submitDateTime ? new Date(d.submitDateTime).toISOString() : null,
          link: d.docID ? `https://disclosure2.edinet-fsa.go.jp/WEEK0010.aspx?docID=${encodeURIComponent(d.docID)}` : null,
          tags: ['filing', d.formCode ? `form:${d.formCode}` : null].filter(Boolean),
          properties: {
            doc_id: d.docID || null,
            form: d.formCode || null,
            doc_type_code: d.docTypeCode || null,
            sec_code: d.secCode || null,
            edinet_code: d.edinetCode || null,
            submit_date: d.submitDateTime || null,
          },
        }));
        live = items.length > 0;
      }
    } catch { /* fall through to seed */ }
  }

  if (items.length === 0) {
    items = [
      { doc: 'seed_edinet_1', filer: 'Toyota Motor', form: '030000', date: today() },
      { doc: 'seed_edinet_2', filer: 'Sony Group',   form: '030000', date: today() },
    ].map((d) => ({
      uid: intelUid(SOURCE_ID, d.doc),
      title: `${d.filer} — seed filing`,
      summary: 'Seed entry (no EDINET API key configured)',
      author: d.filer,
      language: 'ja',
      published_at: new Date(`${d.date}T00:00:00Z`).toISOString(),
      tags: ['filing', 'seed', `form:${d.form}`],
      properties: { doc_id: d.doc, form: d.form, submit_date: d.date, seed: true },
    }));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'FSA EDINET corporate securities disclosures',
    extraMeta: { key_required: true },
  });
}
