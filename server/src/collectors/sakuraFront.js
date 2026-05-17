/**
 * Cherry blossom front (桜前線) — sakura-front.
 *
 * JMA conducts 生物季節観測 (phenological observation) of the cherry-blossom
 * 標本木 (sample tree) at its district observatories and publishes the
 * flowering (開花) / full-bloom (満開) dates. JMA does NOT expose this as a
 * documented JSON API; the public listing is at:
 *   https://www.data.jma.go.jp/sakura/data/sakura_3monthmean.html
 * and the per-year observation table at the seasonal-observation pages.
 *
 * This collector does a real best-effort fetch of the JMA sakura listing and
 * extracts station + flowering-date rows, geocoding each observatory city to
 * coordinates via GSI's key-free address-search (real geocode, no fabricated
 * coords). On fetch/parse/geocode failure for all rows it returns an HONEST
 * EMPTY FeatureCollection.
 *
 * NOTE: the JMA sakura page is HTML and UNVERIFIED as a stable structure;
 * extraction is defensive and degrades to empty rather than guessing.
 */

import { fetchText } from './_liveHelpers.js';
import { gsiAddressSearch } from '../utils/gsiAddressSearch.js';
import { intelHashKey } from '../utils/intelHelpers.js';

const SAKURA_URL = 'https://www.data.jma.go.jp/sakura/data/sakura_3monthmean.html';

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'JMA cherry-blossom (sakura) flowering observation front',
    },
  };
}

export default async function collectSakuraFront() {
  let html;
  try {
    html = await fetchText(SAKURA_URL, { timeoutMs: 15000 });
  } catch {
    return emptyResult('sakura_front_unavailable');
  }
  if (!html || typeof html !== 'string') {
    return emptyResult('sakura_front_unavailable');
  }

  // Extract table rows: first cell = observatory/city name (Japanese), and a
  // date-like cell (M月D日) = flowering date.
  const rows = [];
  const trRe = /<tr\b[^>]*>([\s\S]*?)<\/tr>/gi;
  let m;
  while ((m = trRe.exec(html)) !== null) {
    const cells = [];
    const tdRe = /<t[dh]\b[^>]*>([\s\S]*?)<\/t[dh]>/gi;
    let c;
    while ((c = tdRe.exec(m[1])) !== null) {
      cells.push(c[1].replace(/<[^>]+>/g, ' ').replace(/&nbsp;/g, ' ').replace(/\s+/g, ' ').trim());
    }
    if (cells.length < 2) continue;
    const name = cells[0];
    if (!name || !/[一-鿿ぁ-ゖ]/.test(name)) continue;
    const dateCell = cells.slice(1).find((x) => /\d+\s*月\s*\d+\s*日|\d{1,2}\/\d{1,2}/.test(x));
    if (!dateCell) continue;
    rows.push({ name, date: dateCell });
  }

  if (rows.length === 0) return emptyResult('sakura_front_unavailable');

  // Cap geocode requests politely; real coords only via GSI (no fabrication).
  const capped = rows.slice(0, 60);
  const features = [];
  for (const r of capped) {
    let hit = null;
    try {
      hit = await gsiAddressSearch(r.name);
    } catch {
      hit = null;
    }
    if (!hit) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [hit.lon, hit.lat] },
      properties: {
        station: r.name,
        flowering_date: r.date,
        observation_id: `SAKURA_${intelHashKey(r.name, r.date)}`,
        link: SAKURA_URL,
        source: 'jma_sakura',
      },
    });
  }

  if (features.length === 0) return emptyResult('sakura_front_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jma_sakura',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JMA cherry-blossom (sakura) flowering observation front',
    },
  };
}
