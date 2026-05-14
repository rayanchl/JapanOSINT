/**
 * Overlay store for API keys.
 *
 * Persists user-edited values in `server/data/api-keys.json` and merges them
 * into `process.env` at boot, so the iOS API-keys tab can mutate keys without
 * forcing a process restart. The original `process.env` is snapshotted at
 * module load so clearing an overlay value can fall back to whatever the
 * `.env`-baked value was (instead of leaving the now-stale overlay value or
 * wiping the var entirely).
 *
 * Reads always go through `process.env[name]` — collectors never reach into
 * this module directly. Writes call `setKey()` which both updates the overlay
 * file and mutates `process.env` so the next collector run picks up the new
 * value.
 */
import { readFileSync, writeFileSync, renameSync, mkdirSync, existsSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const OVERLAY_PATH = resolve(__dirname, '../../data/api-keys.json');

// Snapshot at module-load so a later "clear overlay" can restore the original
// (.env-baked) value. Captured before applyOverlayToEnv() runs.
const ORIGINAL_ENV = { ...process.env };

export function loadOverlay() {
  if (!existsSync(OVERLAY_PATH)) return {};
  try {
    const raw = readFileSync(OVERLAY_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
      return parsed;
    }
    return {};
  } catch (err) {
    console.warn('[apiKeysStore] overlay parse failed:', err.message);
    return {};
  }
}

export function saveOverlay(obj) {
  const dir = dirname(OVERLAY_PATH);
  if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
  // Atomic write: dump to a temp file, rename. fs.rename is atomic on the
  // same filesystem so a crash mid-write can't leave a half-written JSON.
  const tmp = OVERLAY_PATH + '.tmp';
  writeFileSync(tmp, JSON.stringify(obj, null, 2), 'utf8');
  renameSync(tmp, OVERLAY_PATH);
}

/**
 * Merge overlay values into `process.env`. Call once during server bootstrap
 * BEFORE any collector module is imported — collectors that capture
 * `process.env[X]` at module load time would otherwise miss the overlay.
 */
export function applyOverlayToEnv() {
  const overlay = loadOverlay();
  let count = 0;
  for (const [k, v] of Object.entries(overlay)) {
    if (typeof v === 'string' && v.length > 0) {
      process.env[k] = v;
      count += 1;
    }
  }
  if (count > 0) {
    console.log(`[apiKeysStore] applied ${count} overlay key(s) to process.env`);
  }
}

/**
 * Set or clear an overlay key. Empty value clears the entry and restores
 * the .env-baked value (or unsets process.env[name] if there was no .env
 * value originally).
 */
export function setKey(name, value) {
  const overlay = loadOverlay();
  if (typeof value !== 'string' || value.length === 0) {
    delete overlay[name];
    if (ORIGINAL_ENV[name] !== undefined) {
      process.env[name] = ORIGINAL_ENV[name];
    } else {
      delete process.env[name];
    }
  } else {
    overlay[name] = value;
    process.env[name] = value;
  }
  saveOverlay(overlay);
  return {
    name,
    set: typeof process.env[name] === 'string' && process.env[name].length > 0,
    hasOverlay: Object.prototype.hasOwnProperty.call(overlay, name),
  };
}

export function getKeyValue(name) {
  const v = process.env[name];
  return typeof v === 'string' && v.length > 0 ? v : null;
}

export function hasOverlay(name) {
  return Object.prototype.hasOwnProperty.call(loadOverlay(), name);
}
