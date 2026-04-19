/**
 * Sparse "active tag"-style labels that float along each railway line.
 *
 * OSM splits a single real-world line (e.g. Yamanote) into dozens of way
 * fragments. We group all LineString features that share the same identity
 * (line color + ref/name) into one logical line, then sample a small number
 * of points along it and attach a styled DOM marker at each. The styling
 * mirrors the "active tag" used by MapPopup (rounded pill, translucent
 * colored background, colored text).
 */

import maplibregl from 'maplibre-gl';

const MIN_ZOOM = 10; // below this, labels are hidden to avoid clutter
const MAX_TAGS_PER_LINE = 3; // sparse — 1 at each third along the line
const MIN_LINE_LENGTH_KM = 1.5; // skip tiny sidings/branches

function haversineKm(a, b) {
  const R = 6371;
  const toRad = (d) => (d * Math.PI) / 180;
  const dLat = toRad(b[1] - a[1]);
  const dLon = toRad(b[0] - a[0]);
  const lat1 = toRad(a[1]);
  const lat2 = toRad(b[1]);
  const h =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(h));
}

function cumulativeLengths(coords) {
  const cum = [0];
  for (let i = 1; i < coords.length; i++) {
    cum.push(cum[i - 1] + haversineKm(coords[i - 1], coords[i]));
  }
  return cum;
}

// Interpolate a point at distance `target` km along a polyline.
function pointAtDistance(coords, cum, target) {
  if (target <= 0) return coords[0];
  const total = cum[cum.length - 1];
  if (target >= total) return coords[coords.length - 1];
  let lo = 0;
  let hi = cum.length - 1;
  while (lo + 1 < hi) {
    const mid = (lo + hi) >> 1;
    if (cum[mid] <= target) lo = mid; else hi = mid;
  }
  const segLen = cum[hi] - cum[lo];
  if (segLen === 0) return coords[lo];
  const t = (target - cum[lo]) / segLen;
  return [
    coords[lo][0] + t * (coords[hi][0] - coords[lo][0]),
    coords[lo][1] + t * (coords[hi][1] - coords[lo][1]),
  ];
}

function identity(feature) {
  const p = feature.properties || {};
  const color = p.line_color || null;
  const label = p.line_ref || p.name || p.name_ja || null;
  if (!color || !label) return null;
  return { key: `${color}::${label}`, color, label };
}

function buildTagElement(color, label) {
  const el = document.createElement('div');
  // Mirror the active tag from MapPopup: rounded pill, translucent colored
  // background at ~20% alpha, full-color text, small monospace.
  el.className = 'railway-line-tag';
  // 33 hex = ~20% alpha. Works for #rrggbb; for #rgb the collector already
  // normalized upstream.
  const bgColor = color.length === 7 ? `${color}33` : color;
  el.style.cssText = [
    'pointer-events: none',
    `background: ${bgColor}`,
    `color: ${color}`,
    `border: 1px solid ${color}`,
    'padding: 1px 6px',
    'border-radius: 9999px',
    'font: 10px ui-monospace, SFMono-Regular, Menlo, monospace',
    'font-weight: 600',
    'white-space: nowrap',
    'letter-spacing: 0.02em',
    'text-shadow: 0 0 4px rgba(0,0,0,0.65)',
    'user-select: none',
  ].join('; ');
  el.textContent = label;
  return el;
}

/**
 * Attach tag markers to the map for the given feature collection.
 * Returns a handle with `destroy()` that removes all markers and listeners.
 */
export function createRailwayLineTags(map, geojson) {
  if (!map || !geojson || !Array.isArray(geojson.features)) {
    return { destroy: () => {} };
  }

  // Group LineString fragments by logical line identity.
  const groups = new Map();
  for (const f of geojson.features) {
    if (!f || !f.geometry) continue;
    if (f.geometry.type !== 'LineString') continue;
    const coords = f.geometry.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) continue;
    const id = identity(f);
    if (!id) continue;
    let g = groups.get(id.key);
    if (!g) {
      g = { color: id.color, label: id.label, fragments: [] };
      groups.set(id.key, g);
    }
    g.fragments.push(coords);
  }

  // Compute sample positions per group: pick the longest fragment (most
  // representative geometry) and place tags at evenly-spaced fractions of
  // its length.
  const tagPositions = [];
  for (const g of groups.values()) {
    let longest = null;
    let longestLen = 0;
    let longestCum = null;
    for (const coords of g.fragments) {
      const cum = cumulativeLengths(coords);
      const len = cum[cum.length - 1];
      if (len > longestLen) {
        longest = coords;
        longestLen = len;
        longestCum = cum;
      }
    }
    if (!longest || longestLen < MIN_LINE_LENGTH_KM) continue;

    for (let i = 1; i <= MAX_TAGS_PER_LINE; i++) {
      const frac = i / (MAX_TAGS_PER_LINE + 1);
      const target = longestLen * frac;
      const pt = pointAtDistance(longest, longestCum, target);
      tagPositions.push({ color: g.color, label: g.label, lngLat: pt });
    }
  }

  // Create markers, but only add them when zoom permits — toggle via
  // zoom listener to avoid mounting hundreds of DOM nodes at low zoom.
  const markers = tagPositions.map(({ color, label, lngLat }) => {
    const el = buildTagElement(color, label);
    return new maplibregl.Marker({ element: el, anchor: 'center' })
      .setLngLat(lngLat);
  });

  let mounted = false;
  const mount = () => {
    if (mounted) return;
    for (const m of markers) m.addTo(map);
    mounted = true;
  };
  const unmount = () => {
    if (!mounted) return;
    for (const m of markers) m.remove();
    mounted = false;
  };
  const onZoom = () => {
    if (map.getZoom() >= MIN_ZOOM) mount(); else unmount();
  };

  onZoom(); // initial
  map.on('zoom', onZoom);

  return {
    destroy: () => {
      map.off('zoom', onZoom);
      unmount();
    },
  };
}
