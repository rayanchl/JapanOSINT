/**
 * Kuromoji wrapper used by intelStore for ingest-time JA segmentation.
 *
 * unicode61 (SQLite's default FTS5 tokenizer) doesn't segment Japanese; it
 * indexes long phrases as opaque single tokens. We pre-segment text with
 * kuromoji into space-delimited morphemes so unicode61 sees discrete tokens
 * on both index and query side. Search query goes through the same
 * `segmentForFts` before MATCH.
 *
 * Tokenizer init is lazy and cached (one ~1 s dictionary load per process).
 * Latin-only strings skip kuromoji and pass through unchanged.
 */

import kuromoji from 'kuromoji';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
// kuromoji ships its IPADIC inside `<pkg>/dict/`. Resolve from this file's
// location (server/src/utils) → ../../node_modules/kuromoji/dict.
const DEFAULT_DICT = path.resolve(__dirname, '..', '..', 'node_modules', 'kuromoji', 'dict');

let tokenizerPromise = null;
// Cached resolved tokenizer for sync callers (cameraStore upsert is invoked
// inside a sync db.transaction). Populated when ensureTokenizer() resolves.
let tokenizerSync = null;

export function ensureTokenizer() {
  if (!tokenizerPromise) {
    tokenizerPromise = new Promise((resolve, reject) => {
      kuromoji.builder({ dicPath: DEFAULT_DICT }).build((err, tk) => {
        if (err) reject(err);
        else resolve(tk);
      });
    }).then((tk) => {
      tokenizerSync = tk;
      return tk;
    }).catch((err) => {
      // Reset so a future call can retry. Log once.
      console.warn('[jpTokenizer] kuromoji init failed:', err?.message);
      tokenizerPromise = null;
      return null;
    });
  }
  return tokenizerPromise;
}

const JP_RE = /[぀-ゟ゠-ヿ一-鿿㐀-䶿ｦ-ﾟ]/;

export function hasJapanese(text) {
  return typeof text === 'string' && JP_RE.test(text);
}

/**
 * Segment text for FTS indexing. Latin-only text passes through unchanged
 * (unicode61 already handles English/European fine). Japanese-bearing text
 * gets kuromoji morpheme split joined with spaces.
 *
 * Returns '' for null/empty/undefined. On tokenizer failure falls back to
 * the input unchanged so search still works at the unicode61 level.
 */
export async function segmentForFts(text) {
  if (!text) return '';
  if (!hasJapanese(text)) return text;
  const tk = await ensureTokenizer();
  if (!tk) return text;
  try {
    return tk.tokenize(text).map((t) => t.surface_form).join(' ');
  } catch (err) {
    console.warn('[jpTokenizer] tokenize failed:', err?.message);
    return text;
  }
}

/**
 * Sync variant for callers that run inside a sync db.transaction (e.g.
 * cameraStore.upsertCamera, transportStore upserts). index.js gates
 * startScheduler() on ensureTokenizer(), so every collector that could
 * reach this has a warm tokenizer. The raw-text fallback below is
 * defense-in-depth — if it ever fires, it means the boot order was broken.
 */
export function segmentForFtsSync(text) {
  if (!text) return '';
  if (!hasJapanese(text)) return text;
  if (!tokenizerSync) return text;
  try {
    return tokenizerSync.tokenize(text).map((t) => t.surface_form).join(' ');
  } catch (err) {
    console.warn('[jpTokenizer] tokenize (sync) failed:', err?.message);
    return text;
  }
}
