/**
 * Geofabrik Japan Extract Ingestion
 * ----------------------------------
 * Downloads the Japan OSM PBF extract from Geofabrik and keeps it on disk so
 * collectors can fall back to a local bulk source when Overpass is rate-limited
 * or unreachable. Does not parse the PBF directly (that would require a native
 * binding); instead it:
 *
 *   1. Streams the download to `<DATA_DIR>/japan-latest.osm.pbf`
 *   2. Records metadata (fetchedAt, size, md5) in `japan-latest.meta.json`
 *   3. Provides `isExtractFresh()` / `getExtractInfo()` for other code to check
 *   4. Provides `extractWithOsmium(filter, outPath)` that optionally invokes
 *      the `osmium` CLI if it exists on PATH; otherwise returns null.
 *
 * Trigger the download via:
 *   node -e "import('./src/utils/geofabrikExtract.js').then(m => m.downloadExtract())"
 * or via the `/api/sources/geofabrik/refresh` admin endpoint.
 */

import { createWriteStream, existsSync, statSync, readFileSync, writeFileSync, mkdirSync, renameSync } from 'fs';
import { dirname, join } from 'path';
import { spawnSync } from 'child_process';

export const GEOFABRIK_URL = 'https://download.geofabrik.de/asia/japan-latest.osm.pbf';
export const GEOFABRIK_MD5_URL = 'https://download.geofabrik.de/asia/japan-latest.osm.pbf.md5';

// Extract is written under the server's data dir (configurable via env).
const DATA_DIR = process.env.OSM_EXTRACT_DIR || join(process.cwd(), 'data', 'osm');
const EXTRACT_PATH = join(DATA_DIR, 'japan-latest.osm.pbf');
const META_PATH = join(DATA_DIR, 'japan-latest.meta.json');

// Geofabrik refreshes the daily extract; we consider 7 days "fresh enough".
const FRESH_TTL_MS = 7 * 24 * 60 * 60 * 1000;

function ensureDataDir() {
  if (!existsSync(DATA_DIR)) mkdirSync(DATA_DIR, { recursive: true });
}

/**
 * Read extract metadata from the sidecar file. Returns null if absent.
 */
export function getExtractInfo() {
  if (!existsSync(META_PATH)) return null;
  try {
    const raw = readFileSync(META_PATH, 'utf8');
    const meta = JSON.parse(raw);
    return {
      ...meta,
      path: EXTRACT_PATH,
      exists: existsSync(EXTRACT_PATH),
      sizeBytes: existsSync(EXTRACT_PATH) ? statSync(EXTRACT_PATH).size : 0,
    };
  } catch {
    return null;
  }
}

/**
 * True if a local extract exists and was fetched within FRESH_TTL_MS.
 */
export function isExtractFresh() {
  const info = getExtractInfo();
  if (!info || !info.exists || !info.fetchedAt) return false;
  const age = Date.now() - new Date(info.fetchedAt).getTime();
  return age < FRESH_TTL_MS;
}

/**
 * Download the Geofabrik Japan PBF extract to the local data dir.
 * Returns { ok, bytes, path } or { ok: false, error }.
 */
export async function downloadExtract({ force = false } = {}) {
  if (!force && isExtractFresh()) {
    return { ok: true, skipped: true, path: EXTRACT_PATH, reason: 'fresh' };
  }
  ensureDataDir();

  try {
    const res = await fetch(GEOFABRIK_URL, {
      headers: { 'User-Agent': 'JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)' },
    });
    if (!res.ok || !res.body) {
      return { ok: false, error: `HTTP ${res.status}` };
    }

    // Stream to disk so we don't buffer ~2 GB in RAM.
    const tmpPath = EXTRACT_PATH + '.tmp';
    const fileStream = createWriteStream(tmpPath);
    const reader = res.body.getReader();
    let bytes = 0;

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      bytes += value.byteLength;
      if (!fileStream.write(Buffer.from(value))) {
        await new Promise((r) => fileStream.once('drain', r));
      }
    }
    await new Promise((r) => fileStream.end(r));

    // Atomic rename on success.
    renameSync(tmpPath, EXTRACT_PATH);

    // Grab the published MD5 for integrity tracking (non-blocking).
    let md5 = null;
    try {
      const md5Res = await fetch(GEOFABRIK_MD5_URL);
      if (md5Res.ok) md5 = (await md5Res.text()).trim().split(/\s+/)[0] || null;
    } catch { /* non-fatal */ }

    const meta = {
      url: GEOFABRIK_URL,
      fetchedAt: new Date().toISOString(),
      sizeBytes: bytes,
      md5,
    };
    writeFileSync(META_PATH, JSON.stringify(meta, null, 2));

    return { ok: true, path: EXTRACT_PATH, bytes, md5 };
  } catch (err) {
    return { ok: false, error: err.message };
  }
}

/**
 * If the `osmium` CLI is on PATH, filter the local extract down to a tag
 * selector (e.g. "n/amenity=parking") and write GeoJSON to `outPath`.
 * Returns true on success, false if osmium is unavailable or the call fails.
 * Collectors can use this for offline fallback when Overpass is down.
 */
export function extractWithOsmium(tagFilter, outPath) {
  if (!existsSync(EXTRACT_PATH)) return false;
  const probe = spawnSync('osmium', ['--version']);
  if (probe.status !== 0) return false;

  const tmpOut = outPath + '.tmp';
  const filter = spawnSync('osmium', [
    'tags-filter', EXTRACT_PATH, tagFilter,
    '-o', tmpOut, '--overwrite', '-f', 'geojson',
  ], { stdio: 'inherit' });

  if (filter.status !== 0) return false;
  try {
    renameSync(tmpOut, outPath);
    return true;
  } catch {
    return false;
  }
}

export const GEOFABRIK_PATHS = { DATA_DIR, EXTRACT_PATH, META_PATH };
