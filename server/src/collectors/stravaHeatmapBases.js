/**
 * Strava Heatmap — sample tile coverage around USFJ + JSDF bases.
 *
 * The 2018 Strava heatmap incident showed that aggregated GPS exercise tracks
 * leak base layouts (perimeters, jogging routes, helipads) for installations
 * that should be opaque. This collector samples z=12 heatmap tiles around a
 * curated list of JP military installations and surfaces *whether* the tile
 * is non-empty (a tile-level signal) — the actual pixels are not stored.
 *
 * Tile URL:
 *   https://heatmap-external-a.strava.com/tiles/all/hot/{z}/{x}/{y}.png
 *   (no auth for "all" activity type at low zoom)
 *
 * Result: one Feature per base, geometry = base lat/lon, properties.tile_size
 * indicates whether activity was detected. Pair with usfjBases / jsdfBases
 * collectors visually.
 */

const TIMEOUT_MS = 12000;
const ZOOM = 12;

// Curated JP installation set. Override with STRAVA_BASES=name|lat|lon;name|...
const DEFAULT_BASES = [
  // USFJ
  { name: 'Yokota Air Base', lat: 35.7486, lon: 139.3486, branch: 'USAF' },
  { name: 'Misawa Air Base', lat: 40.7028, lon: 141.3681, branch: 'USAF' },
  { name: 'Kadena Air Base', lat: 26.3556, lon: 127.7681, branch: 'USAF' },
  { name: 'MCAS Iwakuni', lat: 34.1442, lon: 132.2356, branch: 'USMC' },
  { name: 'MCAS Futenma', lat: 26.2722, lon: 127.7558, branch: 'USMC' },
  { name: 'Camp Schwab', lat: 26.5239, lon: 128.0556, branch: 'USMC' },
  { name: 'Fleet Activities Yokosuka', lat: 35.2917, lon: 139.6611, branch: 'USN' },
  { name: 'NAF Atsugi', lat: 35.4544, lon: 139.4500, branch: 'USN' },
  { name: 'Camp Zama', lat: 35.5111, lon: 139.4017, branch: 'USA' },
  // JSDF
  { name: 'JASDF Hyakuri', lat: 36.1814, lon: 140.4147, branch: 'JASDF' },
  { name: 'JASDF Komaki', lat: 35.2750, lon: 136.9250, branch: 'JASDF' },
  { name: 'JGSDF Camp Asaka', lat: 35.7903, lon: 139.6094, branch: 'JGSDF' },
  { name: 'JGSDF Ichigaya HQ', lat: 35.6906, lon: 139.7300, branch: 'JGSDF' },
  { name: 'JMSDF Yokosuka', lat: 35.2861, lon: 139.6750, branch: 'JMSDF' },
];

function lonToTileX(lon, z) {
  return Math.floor(((lon + 180) / 360) * Math.pow(2, z));
}
function latToTileY(lat, z) {
  const r = (lat * Math.PI) / 180;
  return Math.floor(
    (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2 * Math.pow(2, z),
  );
}

async function probeTile(base) {
  const x = lonToTileX(base.lon, ZOOM);
  const y = latToTileY(base.lat, ZOOM);
  const url = `https://heatmap-external-a.strava.com/tiles/all/hot/${ZOOM}/${x}/${y}.png`;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      signal: controller.signal,
      headers: {
        accept: 'image/png',
        'user-agent': 'Mozilla/5.0 JapanOSINT',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return { ...base, tile_url: url, tile_z: ZOOM, tile_x: x, tile_y: y, ok: false, http: res.status };
    const buf = Buffer.from(await res.arrayBuffer());
    return {
      ...base,
      tile_url: url,
      tile_z: ZOOM,
      tile_x: x,
      tile_y: y,
      ok: true,
      tile_bytes: buf.length,
      // crude empty-tile heuristic: a fully transparent/empty heatmap PNG at
      // strava is consistently small (~600-1500 bytes); active tiles are
      // larger because of additional pixel data. Threshold tuned conservatively.
      activity_detected: buf.length > 1800,
    };
  } catch (err) {
    return { ...base, tile_url: url, tile_z: ZOOM, tile_x: x, tile_y: y, ok: false, error: err?.message };
  }
}

export default async function collectStravaHeatmapBases() {
  const probed = await Promise.all(DEFAULT_BASES.map(probeTile));
  const features = probed.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `STRAVA_${p.name.replace(/\s+/g, '_')}`,
      idx: i,
      name: p.name,
      branch: p.branch,
      tile_z: p.tile_z,
      tile_x: p.tile_x,
      tile_y: p.tile_y,
      tile_url: p.tile_url,
      tile_bytes: p.tile_bytes ?? null,
      activity_detected: p.activity_detected ?? null,
      ok: p.ok,
      error: p.error || null,
      http: p.http || null,
      source: 'strava_heatmap_tile_probe',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'strava_heatmap',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      zoom: ZOOM,
      env_hint: 'Tile bytes >1800 used as activity heuristic; tune for your noise floor',
      description: 'Strava heatmap tile probes around JP military installations',
    },
  };
}
