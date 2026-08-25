/**
 * Deterministic per-satellite color. Both the marker layer and the
 * ground-track line layer call this so a satellite's streak always
 * matches its dot on the map.
 *
 * The palette is 24 saturated hues that read well on the dark basemap.
 */
const PALETTE = [
  '#e53935', '#d81b60', '#8e24aa', '#5e35b1',
  '#3949ab', '#1e88e5', '#039be5', '#00acc1',
  '#00897b', '#43a047', '#7cb342', '#c0ca33',
  '#fdd835', '#ffb300', '#fb8c00', '#f4511e',
  '#6d4c41', '#546e7a', '#ef5350', '#ec407a',
  '#ab47bc', '#5c6bc0', '#26a69a', '#9ccc65',
];

const FALLBACK = '#ba68c8';

export function satelliteColor(noradId) {
  if (noradId === null || noradId === undefined || noradId === '') return FALLBACK;
  const n = typeof noradId === 'number' ? noradId : parseInt(noradId, 10);
  if (!Number.isFinite(n)) return FALLBACK;
  // FNV-1a-ish small hash, enough for palette distribution.
  let h = 0x811c9dc5;
  const s = String(n);
  for (let i = 0; i < s.length; i += 1) {
    h ^= s.charCodeAt(i);
    h = (h * 0x01000193) >>> 0;
  }
  return PALETTE[h % PALETTE.length];
}
