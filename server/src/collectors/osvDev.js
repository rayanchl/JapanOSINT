/**
 * OSV.dev — open-source vulnerability DB (Google).
 *
 * Aggregates GHSA, PYSEC, RUSTSEC, GO-SEC and friends. We hit the queryBatch
 * API with packages commonly maintained by JP authors / popular in the JP
 * dev scene (Ruby on Rails port, Misskey-related npm, etc.) plus a few
 * generic anchors so the layer is never empty when those quiet down.
 *
 * Endpoint: POST https://api.osv.dev/v1/querybatch — free, no auth.
 */

const URL_V1_QUERY = 'https://api.osv.dev/v1/querybatch';
const TIMEOUT_MS = 15000;

// Packages biased toward JP-maintained / JP-popular projects.
// Override at deploy with OSV_PACKAGES="ecosystem:name,..." (semicolons).
const DEFAULT_PACKAGES = (process.env.OSV_PACKAGES || [
  'npm:misskey-js',
  'npm:misskey-reversi',
  'npm:cybozu-front-lib',
  'npm:typescript',         // baseline anchor
  'PyPI:django',            // baseline
  'PyPI:requests',          // baseline
  'Go:github.com/yuin/goldmark',
  'Go:github.com/labstack/echo',
  'Go:github.com/gorilla/mux',
  'RubyGems:rails',
  'RubyGems:devise',
  'RubyGems:nokogiri',
  'Maven:com.fasterxml.jackson.core:jackson-databind',
  'Packagist:laravel/framework',
  'crates.io:tokio',
].join(';')).split(';').map((s) => s.trim()).filter(Boolean);

const TOKYO = [139.6917, 35.6895];

function parsePackage(spec) {
  const [eco, ...rest] = spec.split(':');
  return { ecosystem: eco, name: rest.join(':') };
}

export default async function collectOsvDev() {
  const queries = DEFAULT_PACKAGES.map((spec) => {
    const { ecosystem, name } = parsePackage(spec);
    return { package: { name, ecosystem } };
  });

  let vulnGroups = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL_V1_QUERY, {
      signal: ctrl.signal,
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ queries }),
    });
    clearTimeout(timer);
    if (res.ok) {
      const json = await res.json();
      vulnGroups = Array.isArray(json?.results) ? json.results : [];
      live = true;
    }
  } catch { /* fall through */ }

  const features = [];
  vulnGroups.forEach((group, qi) => {
    const pkg = DEFAULT_PACKAGES[qi];
    const ids = Array.isArray(group?.vulns) ? group.vulns : [];
    ids.slice(0, 6).forEach((v, i) => {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: features.length,
          osv_id: v.id || null,
          modified: v.modified || null,
          package: pkg,
          source: live ? 'osv_querybatch' : 'osv_seed',
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
      packages_polled: DEFAULT_PACKAGES.length,
      env_hint: 'OSV_PACKAGES="ecosystem:name;ecosystem:name" to customise',
      description: 'OSV.dev open-source vulnerability DB — JP-relevant packages',
    },
  };
}
