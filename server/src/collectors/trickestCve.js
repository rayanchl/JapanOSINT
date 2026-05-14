/**
 * Trickest CVE feed — daily exploit/PoC harvester (alt to PoC-in-GitHub).
 *
 * Repo: https://github.com/trickest/cve. Each year directory contains one
 * `CVE-YYYY-NNNNN.md` per CVE; the markdown body lists PoC repos.
 *
 * Strategy mirrors pocInGithub.js: list current-year directory, sample N,
 * fetch raw markdown and extract PoC repo links.
 */

const REPO_API = 'https://api.github.com/repos/trickest/cve/contents';
const RAW_BASE = 'https://raw.githubusercontent.com/trickest/cve/main';
const TIMEOUT_MS = 15000;
const TOKYO = [139.6917, 35.6895];

const SAMPLE_LIMIT = Number(process.env.TRICKEST_CVE_LIMIT || 50);

async function ghJson(url) {
  const token = process.env.GITHUB_TOKEN || '';
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
  const res = await fetch(url, {
    signal: ctrl.signal,
    headers: {
      accept: 'application/vnd.github+json',
      'user-agent': 'japanosint-collector',
      ...(token ? { authorization: `Bearer ${token}` } : {}),
    },
  });
  clearTimeout(timer);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return await res.json();
}

async function fetchText(url) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return '';
    return await r.text();
  } catch { return ''; }
}

function parsePocRepos(md) {
  const out = [];
  // Trickest format: bullet-listed bare URLs under "#### Github".
  const re = /https:\/\/github\.com\/[^\s)<>"]+/g;
  const seen = new Set();
  let m;
  while ((m = re.exec(md)) !== null) {
    const url = m[0].replace(/[.,]+$/, '');
    if (seen.has(url)) continue;
    seen.add(url);
    // Skip the cve.mitre.org reference shield link case
    if (/img\.shields\.io|cve\.mitre\.org/.test(url)) continue;
    const label = url.replace('https://github.com/', '');
    out.push({ label, url });
    if (out.length >= 6) break;
  }
  return out;
}

export default async function collectTrickestCve() {
  const year = new Date().getUTCFullYear();
  let listing = [];
  let live = false;
  for (const y of [year, year - 1]) {
    try {
      listing = await ghJson(`${REPO_API}/${y}?per_page=100`);
      if (Array.isArray(listing) && listing.length) {
        live = true; break;
      }
    } catch { listing = []; }
  }

  const filenames = (Array.isArray(listing) ? listing : [])
    .filter((f) => f?.type === 'file' && /^CVE-\d{4}-\d+\.md$/.test(f.name || ''))
    .sort((a, b) => b.name.localeCompare(a.name))
    .slice(0, SAMPLE_LIMIT)
    .map((f) => ({ name: f.name, path: f.path }));

  const concurrency = 6;
  const out = [];
  for (let i = 0; i < filenames.length; i += concurrency) {
    const slice = filenames.slice(i, i + concurrency);
    const results = await Promise.all(slice.map((f) => fetchText(`${RAW_BASE}/${f.path}`)));
    out.push(...results);
  }

  const features = [];
  out.forEach((md, i) => {
    if (!md) return;
    const cveId = filenames[i].name.replace(/\.md$/, '');
    parsePocRepos(md).forEach((r) => {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: features.length,
          cve_id: cveId,
          repo_label: r.label,
          repo_url: r.url,
          source: live ? 'trickest_cve' : 'trickest_cve_seed',
        },
      });
    });
  });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: live ? 'live' : 'seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      cves_polled: filenames.length,
      env_hint: 'TRICKEST_CVE_LIMIT to tune; GITHUB_TOKEN raises rate limit',
      description: 'Trickest CVE — daily exploit / PoC harvester (alt to nomi-sec)',
    },
  };
}
