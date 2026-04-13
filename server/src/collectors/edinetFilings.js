/**
 * FSA EDINET corporate filings (securities disclosures)
 * https://api.edinet-fsa.go.jp/api/v2/documents.json
 * Requires Subscription-Key header. Without a key we return a seed envelope.
 */

const TIMEOUT_MS = 10000;
const API_KEY = process.env.EDINET_API_KEY;

function today() {
  return new Date().toISOString().slice(0, 10);
}

export default async function collectEdinetFilings() {
  let features = [];
  let source = 'seed';
  if (API_KEY) {
    try {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
      const url = `https://api.edinet-fsa.go.jp/api/v2/documents.json?date=${today()}&type=2&Subscription-Key=${encodeURIComponent(API_KEY)}`;
      const res = await fetch(url, { signal: controller.signal });
      clearTimeout(timer);
      if (res.ok) {
        const data = await res.json();
        const list = data?.results ?? [];
        features = list.slice(0, 200).map((d, i) => ({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [139.77, 35.68] }, // placeholder: Tokyo HQ
          properties: {
            doc_id: d.docID ?? `edinet_${i}`,
            filer: d.filerName ?? null,
            form: d.formCode ?? null,
            doc_type_code: d.docTypeCode ?? null,
            submit_date: d.submitDateTime ?? null,
            description: d.docDescription ?? null,
            source: 'edinet',
          },
        }));
        if (features.length) source = 'live';
      }
    } catch { /* fall through */ }
  }
  if (features.length === 0) {
    features = [
      { doc: 'seed_edinet_1', filer: 'Toyota Motor', form: '030000', date: today() },
      { doc: 'seed_edinet_2', filer: 'Sony Group', form: '030000', date: today() },
    ].map(d => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.77, 35.68] },
      properties: { doc_id: d.doc, filer: d.filer, form: d.form, submit_date: d.date, source: 'edinet_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'FSA EDINET corporate securities disclosures',
      key_required: true,
    },
    metadata: {},
  };
}
