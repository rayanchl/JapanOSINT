/**
 * MOJ White Paper on Crime (法務省 犯罪白書).
 *
 * Source: https://hakusyo1.moj.go.jp/ — the Ministry of Justice's annual
 * crime white paper. Complements NPA crime statistics with the downstream
 * justice-system view: prosecution rates, sentencing patterns, recidivism,
 * juvenile justice, prison population, foreign-national prosecutions.
 *
 * The site publishes one HTML edition per year plus PDF downloads. This
 * collector is intel-first — there are no obvious geocodable rows — and
 * emits a single map pin at MOJ HQ in Tokyo for visibility, plus intel
 * items linking the latest English and Japanese editions.
 */

import { fetchHead, fetchText } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'moj-crime-whitepaper';
// MOJ HQ (法務省) — 千代田区霞が関1-1-1
const MOJ_HQ = { lat: 35.6735, lon: 139.7531 };

// MOJ uses a stable "edition number" in the path: edition 73 = 2024 (令和6),
// 72 = 2023, etc. Generate URLs for the latest few editions and probe each
// for reachability so we always surface the freshest available edition.
function editionsToTry() {
  // Edition n was published in (1951 + n). Edition 72 → 2023, 73 → 2024.
  // Each edition publishes multiple HTML files; we probe a couple of stable
  // names because `mokuji.html` doesn't exist on every edition.
  // Probe a 4-year window centred on the current calendar year. The JP and
  // EN editions don't always release together (EN is sometimes ahead of JP
  // for a few months), and editions occasionally skip mokuji.html in
  // favour of a chapter-1 entry, so we cast a wider net.
  const out = [];
  const thisYear = new Date().getFullYear();
  for (let edition = thisYear - 1951 + 1; edition >= thisYear - 1951 - 4; edition--) {
    out.push({
      edition,
      year: 1951 + edition,
      jaCandidates: [
        `https://hakusyo1.moj.go.jp/jp/${edition}/nfm/mokuji.html`,
        `https://hakusyo1.moj.go.jp/jp/${edition}/nfm/n_${edition}_1_1_1_0_0.html`,
      ],
      enCandidates: [
        `https://hakusyo1.moj.go.jp/en/${edition}/nfm/mokuji.html`,
        `https://hakusyo1.moj.go.jp/en/${edition}/nfm/n_${edition}_1_2_0_0_0.html`,
      ],
    });
  }
  return out;
}

async function probe(url) {
  // MOJ rejects HEAD requests; try HEAD first, then a small GET fallback.
  try {
    const ok = await fetchHead(url, { timeoutMs: 5000 });
    if (ok) return true;
  } catch { /* fall through */ }
  try {
    const text = await fetchText(url, { timeoutMs: 6000 });
    return !!(text && text.length > 200);
  } catch {
    return false;
  }
}

async function firstReachable(urls) {
  for (const u of urls) {
    if (await probe(u)) return u;
  }
  return null;
}

async function findLatestEdition() {
  for (const e of editionsToTry()) {
    const jaUrl = await firstReachable(e.jaCandidates);
    if (jaUrl) {
      const enUrl = await firstReachable(e.enCandidates);
      return {
        edition: e.edition,
        year: e.year,
        jaUrl,
        enUrl,
        jaReachable: true,
        enReachable: !!enUrl,
      };
    }
  }
  return null;
}

export default async function collectMojCrimeWhitepaper() {
  const fetchedAt = new Date().toISOString();
  const latest = await findLatestEdition();

  if (!latest) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: SOURCE_ID,
        fetchedAt,
        recordCount: 0,
        live: false,
        live_source: null,
        upstream_url: 'https://hakusyo1.moj.go.jp/',
        description: 'MOJ White Paper on Crime — upstream unreachable.',
      },
      intel: { items: [] },
    };
  }

  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [MOJ_HQ.lon, MOJ_HQ.lat] },
    properties: {
      id: `MOJ_WP_${latest.year}`,
      name: `MOJ White Paper on Crime (${latest.year})`,
      year: latest.year,
      edition: latest.edition,
      ja_url: latest.jaUrl,
      en_url: latest.enUrl,
      year_month: `${latest.year}-12`,
      source: SOURCE_ID,
    },
  }];

  const intelItems = [
    {
      uid: intelUid(SOURCE_ID, `ja_${latest.edition}`),
      title: `犯罪白書 ${latest.year}年版 (Edition ${latest.edition}) — 日本語`,
      summary: 'MOJ annual crime white paper — prosecution rates, sentencing, recidivism, juvenile justice, prison population.',
      link: latest.jaUrl,
      language: 'ja',
      published_at: fetchedAt,
      tags: ['crime', 'justice', 'white-paper', 'moj', 'national'],
      properties: {
        publisher: 'Ministry of Justice (法務省)',
        edition: latest.edition,
        year: latest.year,
        landing: latest.jaUrl,
      },
    },
  ];
  if (latest.enReachable) {
    intelItems.push({
      uid: intelUid(SOURCE_ID, `en_${latest.edition}`),
      title: `White Paper on Crime ${latest.year} (English Edition ${latest.edition})`,
      summary: 'English edition of MOJ annual crime white paper.',
      link: latest.enUrl,
      language: 'en',
      published_at: fetchedAt,
      tags: ['crime', 'justice', 'white-paper', 'moj', 'national', 'english'],
      properties: {
        publisher: 'Ministry of Justice (法務省)',
        edition: latest.edition,
        year: latest.year,
        landing: latest.enUrl,
      },
    });
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: true,
      live_source: 'moj_hakusyo',
      upstream_url: latest.jaUrl,
      latest_edition: latest.edition,
      latest_year: latest.year,
      description: 'MOJ annual crime white paper directory (single map pin at MOJ HQ; full report content is in the linked HTML/PDF).',
    },
    intel: { items: intelItems },
  };
}
