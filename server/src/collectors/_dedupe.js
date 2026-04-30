/**
 * Shared deduplication helpers for fused transit + AIS collectors.
 *
 * dedupeByKeys(features, keyFns) — first non-null key value wins the slot.
 * dedupeByCoordGrid(features, precision) — bucket features by rounded lat/lon.
 * mergeFeatureCollections(results) — flatten a list of GeoJSON collections
 *                                    into a single feature array.
 */

export function mergeFeatureCollections(results) {
  const out = [];
  for (const r of results) {
    if (!r) continue;
    const feats = Array.isArray(r) ? r
      : Array.isArray(r.features) ? r.features
      : [];
    for (const f of feats) {
      if (f && f.geometry && f.properties) out.push(f);
    }
  }
  return out;
}

// Katakana → hiragana: each katakana codepoint is exactly 0x60 higher than
// its hiragana counterpart, so a single subtract on the BMP range 0x30A1…
// 0x30F6 collapses half the script pair. Full-width katakana small
// characters and the prolonged-sound mark fold the same way; we don't
// bother with half-width katakana because NFKC normalisation below upgrades
// those to full-width first.
function katakanaToHiragana(s) {
  let out = '';
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    if (c >= 0x30A1 && c <= 0x30F6) {
      out += String.fromCharCode(c - 0x60);
    } else {
      out += s[i];
    }
  }
  return out;
}

/** Normalise a Japanese station/stop name for fuzzy matching. */
export function normName(raw) {
  if (!raw) return '';
  return String(raw)
    .normalize('NFKC')
    .toLowerCase()
    .replace(/駅|station|停留所|バス停|stop/gi, '')
    .replace(/[\s・\-()（）、,.]/g, '')
    .trim();
}

/**
 * Stricter fingerprint for cross-mode station clustering. Builds on normName
 * and additionally strips the common Japanese station-suffix particles
 * (〜前・〜口・〜入口) that OSM and MLIT disagree on, then folds katakana
 * to hiragana so "シンジュク"/"しんじゅく" hash to the same key.
 */
export function stationNameFingerprint(raw) {
  if (!raw) return '';
  let s = normName(raw);
  // Strip trailing position particles even after normName's 駅/station pass:
  // 〜前 ("in front of"), 〜口 ("-mouth"/exit), 〜入口 ("entrance").
  s = s.replace(/(入口|前|口)$/u, '');
  return katakanaToHiragana(s);
}

/** Small Levenshtein for sub-3-character drift between name variants. */
export function levenshtein(a, b) {
  if (a === b) return 0;
  if (!a.length) return b.length;
  if (!b.length) return a.length;
  const m = a.length;
  const n = b.length;
  let prev = new Array(n + 1);
  let curr = new Array(n + 1);
  for (let j = 0; j <= n; j++) prev[j] = j;
  for (let i = 1; i <= m; i++) {
    curr[0] = i;
    for (let j = 1; j <= n; j++) {
      const cost = a.charCodeAt(i - 1) === b.charCodeAt(j - 1) ? 0 : 1;
      curr[j] = Math.min(prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost);
    }
    [prev, curr] = [curr, prev];
  }
  return prev[n];
}

/**
 * Dedupe using ordered key functions. The first keyFn that returns a
 * non-empty string becomes the feature's dedup key. Features with no
 * valid key fall through to a coord-grid bucket.
 */
export function dedupeByKeys(features, keyFns, { coordPrecision = 4 } = {}) {
  const seen = new Map(); // key -> feature
  const kept = [];

  for (const f of features) {
    let key = null;
    for (const fn of keyFns) {
      const k = fn(f);
      if (k) { key = k; break; }
    }
    if (!key) {
      const [lon, lat] = f.geometry.coordinates || [];
      if (Number.isFinite(lon) && Number.isFinite(lat)) {
        key = `_c:${lon.toFixed(coordPrecision)},${lat.toFixed(coordPrecision)}:${normName(f.properties?.name)}`;
      } else {
        continue;
      }
    }
    if (seen.has(key)) {
      // Merge properties: fill in nulls from the newer record
      const existing = seen.get(key);
      const merged = { ...existing.properties };
      for (const [k, v] of Object.entries(f.properties || {})) {
        if (merged[k] == null && v != null) merged[k] = v;
      }
      // Track which upstream sources contributed
      const sources = new Set();
      if (existing.properties?.source) sources.add(existing.properties.source);
      if (f.properties?.source) sources.add(f.properties.source);
      merged.sources = Array.from(sources);
      existing.properties = merged;
    } else {
      const copy = {
        type: 'Feature',
        geometry: f.geometry,
        properties: { ...f.properties, sources: [f.properties?.source].filter(Boolean) },
      };
      seen.set(key, copy);
      kept.push(copy);
    }
  }
  return kept;
}

/** Count features by upstream source (uses `sources[]` or falls back to `source`). */
export function countBySource(features) {
  const counts = {};
  for (const f of features) {
    const list = f.properties?.sources
      || (f.properties?.source ? [f.properties.source] : ['unknown']);
    for (const s of list) counts[s] = (counts[s] || 0) + 1;
  }
  return counts;
}
