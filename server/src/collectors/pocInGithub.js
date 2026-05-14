/**
 * PoC-in-GitHub — daily-collected CVE → public PoC mapping (nomi-sec).
 *
 * Pulls the latest year's directory listing via GitHub contents API, then
 * fetches a sample of CVE JSONs to surface working PoCs. Free, no auth.
 *
 * Repo: https://github.com/nomi-sec/PoC-in-GitHub
 * API:  GET https://api.github.com/repos/nomi-sec/PoC-in-GitHub/contents/<year>
 */

const REPO_API = 'https://api.github.com/repos/nomi-sec/PoC-in-GitHub/contents';
const RAW_BASE = 'https://raw.githubusercontent.com/nomi-sec/PoC-in-GitHub/master';
const TIMEOUT_MS = 15000;
const TOKYO = [139.6917, 35.6895];

const SAMPLE_LIMIT = Number(process.env.POC_IN_GITHUB_LIMIT || 60);

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

export default async function collectPocInGithub() {
  const year = new Date().getUTCFullYear();
  let listing = [];
  let live = false;
  try {
    listing = await ghJson(`${REPO_API}/${year}`);
    live = Array.isArray(listing) && listing.length > 0;
  } catch {
    try {
      listing = await ghJson(`${REPO_API}/${year - 1}`);
      live = Array.isArray(listing) && listing.length > 0;
    } catch { listing = []; }
  }

  // Sort by name desc (newer CVE IDs end up later) and slice to a sample.
  const filenames = (Array.isArray(listing) ? listing : [])
    .filter((f) => f?.type === 'file' && /^CVE-\d{4}-\d+\.json$/.test(f.name || ''))
    .sort((a, b) => b.name.localeCompare(a.name))
    .slice(0, SAMPLE_LIMIT)
    .map((f) => f.name);

  const fetchOne = async (name) => {
    try {
      const ctrl = new AbortController();
      const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
      const r = await fetch(`${RAW_BASE}/${name.startsWith('CVE-') ? name.slice(4, 8) : year}/${name}`, {
        signal: ctrl.signal,
      });
      clearTimeout(t);
      if (!r.ok) return null;
      return await r.json();
    } catch { return null; }
  };

  const concurrency = 6;
  const out = [];
  for (let i = 0; i < filenames.length; i += concurrency) {
    const slice = filenames.slice(i, i + concurrency);
    out.push(...(await Promise.all(slice.map(fetchOne))));
  }

  const features = [];
  out.forEach((arr, i) => {
    if (!Array.isArray(arr) || arr.length === 0) return;
    const cveId = filenames[i].replace(/\.json$/, '');
    arr.slice(0, 5).forEach((repo) => {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: features.length,
          cve_id: cveId,
          repo_name: repo.full_name || repo.name || null,
          repo_url: repo.html_url || null,
          stars: repo.stargazers_count ?? null,
          created: repo.created_at || null,
          updated: repo.updated_at || null,
          description: (repo.description || '').slice(0, 400),
          source: live ? 'poc_in_github' : 'poc_in_github_seed',
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
      env_hint: 'GITHUB_TOKEN recommended (raw fetches OK without). POC_IN_GITHUB_LIMIT to tune',
      description: 'PoC-in-GitHub (nomi-sec) — CVE → public PoC mapping, recent N',
    },
  };
}
