/**
 * COVID-19 Japan Dashboard collector (covid19-japan).
 *
 * Most COVID dashboards (toyokeizai, covid19-japan-web-api, JAG-Japan) were
 * decommissioned after the May-2023 reclassification, and routine national
 * reporting ended. We make a best-effort key-free fetch of the MHLW open
 * data CSV that historically lived at
 * https://covid19.mhlw.go.jp/public/opendata/newly_confirmed_cases_daily.csv
 * and, if it still resolves, emit the most recent national row(s). If the
 * endpoint is gone (the common case in 2026), we return an honest empty
 * intel envelope — never fabricated counts.
 *
 * UNVERIFIED ENDPOINT: the MHLW open-data CSV path may have been retired;
 * the collector degrades to empty rather than guessing.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText, parseCsv } from './_liveHelpers.js';

const SOURCE_ID = 'covid19-japan';
const CSV_URL = 'https://covid19.mhlw.go.jp/public/opendata/newly_confirmed_cases_daily.csv';

export default async function collectCovid19Japan() {
  const text = await fetchText(CSV_URL, { timeoutMs: 15000 });
  if (!text) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'covid19_japan_unavailable', note: 'MHLW COVID open data likely retired post-2023 reclassification' },
      description: 'COVID-19 Japan daily cases (MHLW open data — defunct fallback)',
    });
  }

  const rows = parseCsv(text, { headers: true });
  if (!rows.length) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'covid19_japan_unavailable' },
      description: 'COVID-19 Japan daily cases (MHLW open data)',
    });
  }

  // Emit the most recent 30 daily national rows (column "ALL" if present).
  const recent = rows.slice(-30);
  const items = recent.map((r, i) => {
    const date = r.Date || r.date || Object.values(r)[0];
    const all = r.ALL ?? r.All ?? r.all ?? null;
    return {
      uid: intelUid(SOURCE_ID, date || i),
      title: `COVID-19 Japan — ${date ?? '?'}`,
      summary: all != null ? `Newly confirmed (national): ${all}` : 'COVID-19 daily row',
      body: `MHLW newly_confirmed_cases_daily ${date}: ${JSON.stringify(r)}`,
      link: 'https://covid19.mhlw.go.jp/',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['health', 'covid19', 'statistics'],
      properties: { date, national_new_cases: all, row: r },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: { source: 'covid19_japan_live' },
    description: 'COVID-19 Japan daily newly-confirmed cases (MHLW open data)',
  });
}
