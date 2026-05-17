/**
 * JMA Sea of Okhotsk sea-ice collector (jma-ice) — kind:'intel'.
 *
 * Real key-free data file behind the JMA Okhotsk sea-ice diagnostic page
 * (https://www.data.jma.go.jp/kaiyou/shindan/a_1/series_okhotsk/series_okhotsk.html):
 *   https://www.data.jma.go.jp/kaiyou/data/shindan/a_1/series_okhotsk/longterm_okhotsk_latest.txt
 * Tab-separated "SEASON<TAB>MAX (x10^4 km^2)" rows, one per winter season.
 * We emit the latest season's maximum extent (plus a few recent seasons) as
 * an intel item. Honest empty on fetch/parse failure — never fabricated.
 */

import { fetchText } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jma-ice';
const PAGE = 'https://www.data.jma.go.jp/kaiyou/shindan/a_1/series_okhotsk/series_okhotsk.html';
const DATA = 'https://www.data.jma.go.jp/kaiyou/data/shindan/a_1/series_okhotsk/longterm_okhotsk_latest.txt';

export default async function collectJmaIce() {
  const txt = await fetchText(DATA, { timeoutMs: 10000 }).catch(() => null);
  if (!txt) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'JMA Sea of Okhotsk sea-ice maximum extent — upstream unavailable',
    });
  }

  const rows = [];
  for (const line of txt.split(/\r?\n/)) {
    const parts = line.split('\t');
    if (parts.length < 2) continue;
    const season = parts[0].trim();
    const val = parseFloat(parts[1]);
    if (!/^\d{4}-\d{2}$/.test(season) || !Number.isFinite(val)) continue;
    rows.push({ season, max_extent_1e4_km2: val });
  }

  if (rows.length === 0) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'JMA Sea of Okhotsk sea-ice maximum extent — no rows parsed',
    });
  }

  const latest = rows[rows.length - 1];
  const recent = rows.slice(-10);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, latest.season),
      title: `Sea of Okhotsk max sea-ice extent ${latest.season}: ${latest.max_extent_1e4_km2} ×10⁴ km²`,
      summary: `JMA: maximum sea-ice extent for the ${latest.season} winter season was ${latest.max_extent_1e4_km2} ×10⁴ km².`,
      body: `Annual maximum Sea of Okhotsk sea-ice extent (×10⁴ km²). Latest: ${latest.season} = ${latest.max_extent_1e4_km2}. Recent seasons: ${recent.map((r) => `${r.season}:${r.max_extent_1e4_km2}`).join(', ')}.`,
      link: PAGE,
      author: '気象庁 Japan Meteorological Agency',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['sea-ice', 'okhotsk', 'jma', 'climate'],
      properties: {
        latest_season: latest.season,
        latest_max_extent_1e4_km2: latest.max_extent_1e4_km2,
        recent_seasons: recent,
        total_seasons: rows.length,
      },
    }],
    live: true,
    description: 'JMA Sea of Okhotsk annual maximum sea-ice extent (longterm series)',
  });
}
