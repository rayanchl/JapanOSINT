/**
 * Per-line color resolver for rail / subway / tram / monorail features.
 *
 * Strategy:
 *   1. Honor OSM `colour` tag (hex or basic named color) when present.
 *   2. Otherwise hash a stable identifier (line name, ref, operator, network)
 *      into a fixed palette so every distinct line gets a distinct color,
 *      deterministically, without hand-maintained lookups.
 *   3. If nothing hashable is available, return null — callers fall back to
 *      the layer's default color on the client.
 */

const NAMED_COLORS = {
  red: '#e53935',
  blue: '#1e88e5',
  green: '#43a047',
  yellow: '#fdd835',
  orange: '#fb8c00',
  purple: '#8e24aa',
  pink: '#ec407a',
  brown: '#6d4c41',
  grey: '#757575',
  gray: '#757575',
  silver: '#bdbdbd',
  black: '#212121',
  white: '#eeeeee',
  cyan: '#00acc1',
  magenta: '#d81b60',
  lime: '#c0ca33',
  teal: '#00897b',
  navy: '#1a237e',
  maroon: '#b71c1c',
  olive: '#827717',
};

// 24-entry palette generated from golden-ratio hue steps at fixed S/L.
// Hand-tuned to stay readable against both light and dark basemaps.
export const FALLBACK_PALETTE = [
  '#e53935', '#1e88e5', '#43a047', '#fb8c00',
  '#8e24aa', '#00acc1', '#d81b60', '#3949ab',
  '#f4511e', '#7cb342', '#5e35b1', '#00897b',
  '#fdd835', '#6d4c41', '#c0ca33', '#1e6091',
  '#ad1457', '#2e7d32', '#ef6c00', '#4527a0',
  '#ff7043', '#546e7a', '#c2185b', '#00838f',
];

const HEX_RE = /^#([0-9a-f]{3}|[0-9a-f]{6})$/i;

function normalizeHex(raw) {
  const s = raw.trim().toLowerCase();
  if (!HEX_RE.test(s)) return null;
  if (s.length === 4) {
    // #rgb -> #rrggbb
    return `#${s[1]}${s[1]}${s[2]}${s[2]}${s[3]}${s[3]}`;
  }
  return s;
}

function parseColour(raw) {
  if (!raw || typeof raw !== 'string') return null;
  const trimmed = raw.trim();
  if (!trimmed) return null;
  if (trimmed.startsWith('#')) return normalizeHex(trimmed);
  const named = NAMED_COLORS[trimmed.toLowerCase()];
  return named || null;
}

// FNV-1a 32-bit. Fast, no deps, deterministic across Node versions.
function hashString(s) {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = (h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24))) >>> 0;
  }
  return h >>> 0;
}

function hashToColor(key) {
  const idx = hashString(key) % FALLBACK_PALETTE.length;
  return FALLBACK_PALETTE[idx];
}

/**
 * Resolve a color for a railway feature.
 *
 * @param {object} tags - OSM tags object (may be undefined).
 * @param {object} [opts]
 * @param {string[]} [opts.keys] - Ordered list of tag names to try as the
 *   hash input when no `colour` tag is present. Defaults to
 *   ['name', 'ref', 'line', 'network', 'operator'].
 * @returns {string|null} '#rrggbb' or null if no identifier was available.
 */
export function computeLineColor(tags, opts = {}) {
  if (!tags) return null;
  const fromTag = parseColour(tags.colour);
  if (fromTag) return fromTag;

  const keys = opts.keys || ['name', 'ref', 'line', 'network', 'operator'];
  for (const k of keys) {
    const v = tags[k];
    if (v && typeof v === 'string' && v.trim()) {
      return hashToColor(v.trim().toLowerCase());
    }
  }
  return null;
}
