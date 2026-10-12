/**
 * Formatting shared by the source pages (/console/sources, /console/sources/:id,
 * /console/collectors). Pure functions, unit-tested in
 * components/sources/__tests__/sourcePages.test.jsx.
 *
 * The rule every helper follows: a value the server did not measure (null /
 * undefined) renders as "not measured", never as 0, "-" or a default that could
 * be read as a fact.
 */

export const NOT_MEASURED = 'not measured';

/** null/undefined → "not measured"; otherwise the formatter's result. */
export function measured(v, fmt = (x) => String(x)) {
  return v === null || v === undefined ? NOT_MEASURED : fmt(v);
}

/** 0 → "on demand"; seconds → "2 h 2 min" / "1 d" / "45 s". */
export function fmtInterval(sec) {
  if (sec === null || sec === undefined) return NOT_MEASURED;
  const n = Number(sec);
  if (!Number.isFinite(n)) return NOT_MEASURED;
  if (n <= 0) return 'on demand';
  const d = Math.floor(n / 86400), h = Math.floor((n % 86400) / 3600), m = Math.floor((n % 3600) / 60), s = n % 60;
  const parts = [];
  if (d) parts.push(`${d} d`);
  if (h) parts.push(`${h} h`);
  if (m) parts.push(`${m} min`);
  if (!d && !h && s) parts.push(`${s} s`);
  return parts.slice(0, 2).join(' ') || `${n} s`;
}

/** Bytes → "1.2 MB" (1024-based, one decimal above KB). */
export function fmtBytes(n) {
  if (n === null || n === undefined || !Number.isFinite(Number(n))) return NOT_MEASURED;
  const b = Number(n);
  if (b < 1024) return `${b} B`;
  const units = ['KB', 'MB', 'GB', 'TB'];
  let v = b / 1024, i = 0;
  while (v >= 1024 && i < units.length - 1) { v /= 1024; i++; }
  return `${v < 10 ? v.toFixed(1) : Math.round(v)} ${units[i]}`;
}

export function fmtCount(n) {
  if (n === null || n === undefined || !Number.isFinite(Number(n))) return NOT_MEASURED;
  return Number(n).toLocaleString();
}

export function fmtPct(x) {
  if (x === null || x === undefined || !Number.isFinite(Number(x))) return NOT_MEASURED;
  return `${Math.round(Number(x) * 100)}%`;
}

/** The server's `kind` word → a sentence an analyst can act on. */
export const KIND_LABELS = {
  pivot: 'Entity pivot — runs when a search names an entity',
  scheduled: 'Scheduled feed — fetched on its interval',
  'scheduled-pivot': 'Declares an interval but needs an entity — skipped on schedule (rule 3)',
  unreachable: 'Fixed URL with no interval — registered but never runs (rule 3)',
  internal: 'Internal pod — not a data source',
};
export function kindLabel(kind) {
  return KIND_LABELS[kind] || (kind ? String(kind) : NOT_MEASURED);
}

export const ENGINE_LABELS = {
  hpengine: 'hpengine (declarative row)',
  vjson: 'generated JSON feed (VJSON)',
  vrss: 'generated RSS/Atom feed (VRSS)',
  vgeo: 'generated GeoJSON feed (VGEO)',
  vcsv: 'generated CSV feed (VCSV)',
  rss: 'RSS collector macro',
  socrata: 'Socrata registry collector',
  native: 'hand-written run()',
};
export function engineLabel(e) {
  return ENGINE_LABELS[e] || (e ? String(e) : NOT_MEASURED);
}

export function statusTone(status) {
  switch ((status || '').toLowerCase()) {
    case 'online': return 'success';
    case 'degraded': return 'warning';
    case 'offline': return 'danger';
    default: return 'neutral';
  }
}

export function gradeTone(grade) {
  if (!grade) return 'neutral';
  if (grade === 'A' || grade === 'B') return 'success';
  if (grade === 'C') return 'warning';
  return 'danger';
}

/** Highlights {tokens} in a URL/body template so the entity slots stand out. */
export function splitTemplate(tmpl) {
  if (!tmpl) return [];
  const out = [];
  const re = /\{[^}]*\}/g;
  let last = 0, m;
  while ((m = re.exec(tmpl))) {
    if (m.index > last) out.push({ text: tmpl.slice(last, m.index), token: false });
    out.push({ text: m[0], token: true });
    last = m.index + m[0].length;
  }
  if (last < tmpl.length) out.push({ text: tmpl.slice(last), token: false });
  return out;
}

/** The search page's state ↔ URL query, so a filtered list is a shareable link. */
export const SEARCH_KEYS = ['q', 'collector', 'category', 'engine', 'kind', 'status', 'grade', 'has_items', 'sort', 'desc'];
export function searchPath(params, { limit } = {}) {
  const sp = new URLSearchParams();
  for (const k of SEARCH_KEYS) {
    const v = params[k];
    if (v === undefined || v === null || v === '' || v === false || v === 0) continue;
    sp.set(k, v === true ? '1' : String(v));
  }
  if (limit) sp.set('limit', String(limit));
  const qs = sp.toString();
  return `/api/sources/search${qs ? `?${qs}` : ''}`;
}
