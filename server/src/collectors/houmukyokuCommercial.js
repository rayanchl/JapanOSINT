/**
 * Houmukyoku (Legal Affairs Bureau) commercial / corporate registry
 * (houmukyoku-commercial).
 *
 * The 登記情報提供サービス commercial-registration records are a paid,
 * restricted portal (登記情報提供サービス / 商業・法人登記情報). Access
 * requires a contracted API credential. We resolve HOUMUKYOKU_API_KEY via
 * envFor and, when present, query the contracted API. Without the credential
 * we return an honest empty result. We deliberately do NOT scrape the paid
 * portal and never fabricate registry records.
 *
 * Note: there is no free public JSON for full commercial registration. The
 * free National Tax Agency Houjin-Bangou API (houjin-bangou) is a *separate*
 * source and is intentionally not duplicated here.
 *
 * Endpoint (unverified — contracted/paid API; exact base provided with the
 * contract): HOUMUKYOKU_API_BASE (optional override) else a documented
 * placeholder. Credential var: HOUMUKYOKU_API_KEY.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'houmukyoku-commercial';

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Houmukyoku commercial/corporate registry records (paid restricted — requires HOUMUKYOKU_API_KEY)',
    },
  };
}

export default async function collectHoumukyokuCommercial() {
  const apiKey = envFor('HOUMUKYOKU_API_KEY');
  if (!apiKey) return envelope([], 'houmukyoku_commercial_no_key');

  // Contracted API base is provided with the paid contract; allow override.
  const base = envFor('HOUMUKYOKU_API_BASE')
    || 'https://www1.touki.or.jp/api/v1/commercial';
  // Watchlist of corporate numbers / company names to monitor; absent → empty.
  const watch = (envFor('HOUMUKYOKU_WATCHLIST') || '')
    .split(',').map((s) => s.trim()).filter(Boolean);
  if (watch.length === 0) return envelope([], 'houmukyoku_commercial_unavailable');

  const items = [];
  let gotAny = false;

  for (const target of watch) {
    let data = null;
    try {
      const url = `${base}/registration?key=${encodeURIComponent(apiKey)}`
        + `&query=${encodeURIComponent(target)}`;
      data = await fetchJson(url, { timeoutMs: 20000 });
    } catch { data = null; }
    if (!data) continue;
    gotAny = true;

    const records = Array.isArray(data.records) ? data.records
      : Array.isArray(data.results) ? data.results
        : (data.record ? [data.record] : []);
    for (const r of records) {
      const id = r.corporateNumber || r.registrationNumber || r.id || target;
      items.push({
        uid: `${SOURCE_ID}|${id}`,
        title: r.companyName || r.name || String(id),
        summary: r.address || r.headOfficeAddress || null,
        body: [r.companyName, r.address, r.representative, r.businessPurpose]
          .filter(Boolean).join('\n') || null,
        link: r.detailUrl || null,
        author: null,
        language: 'ja',
        published_at: r.registrationDate || r.lastUpdated || null,
        tags: ['houmukyoku', 'commercial-registry', 'corporate'],
        properties: {
          corporate_number: r.corporateNumber ?? null,
          registration_number: r.registrationNumber ?? null,
          representative: r.representative ?? null,
          capital: r.capital ?? null,
          status: r.status ?? null,
          query: target,
          source: 'houmukyoku_api',
        },
      });
    }
  }

  if (!gotAny) return envelope([], 'houmukyoku_commercial_unavailable');
  if (!items.length) return envelope([], 'houmukyoku_commercial_unavailable');
  return envelope(items, 'houmukyoku_api');
}
