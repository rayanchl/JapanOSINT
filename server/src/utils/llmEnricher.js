// server/src/utils/llmEnricher.js
import db from './database.js';
import { chat as defaultChat } from './llmClient.js';
import { gsiAddressSearch as defaultGsi } from './gsiAddressSearch.js';
import { findUncertainStationPairs as defaultPairs } from './stationClusterer.js';
import {
  buildDedupPairPrompt,
  buildSocialGeocodePrompt,
  buildIntelKeywordsPrompt,
} from './llmPrompts.js';
import { updateItemKeywords } from './intelStore.js';
import {
  applyGeocodeOk as applySocialGeocodeOk,
  applyGeocodeFail as applySocialGeocodeFail,
} from './socialPostsStore.js';
import {
  applyGeocodeOk as applyCameraGeocodeOk,
  applyGeocodeFail as applyCameraGeocodeFail,
} from './cameraStore.js';

const DEFAULT_BATCH = Number(process.env.LLM_BATCH_SIZE || 50);
const VISION = process.env.LLM_VISION === 'true';
const TIMEOUT_MS = Number(process.env.LLM_TIMEOUT_MS || 30_000);
const PLACE_CONFIDENCE_GATE = 0.5;
// LLM_VISION_MODEL overrides LLM_MODEL when a prompt actually contains images.
// This lets us run a fast text-only model for the bulk path (Qwen, Llama 3 8B, …)
// and only spin up the bigger multimodal model (Gemma 4, Llava, …) when needed.
const VISION_MODEL = process.env.LLM_VISION_MODEL || null;

// True if the user-message content is an OpenAI multi-part array (i.e. has image_url).
function promptUsesVision(messages) {
  const user = messages.find((m) => m.role === 'user');
  return Array.isArray(user?.content) && user.content.some((p) => p.type === 'image_url');
}
// Dedup pairs are recorded regardless of confidence; the clusterer
// separately gates on >= 0.7 when deciding whether to merge.

const stmtInsertMerge = db.prepare(`
  INSERT OR REPLACE INTO llm_station_merges
    (uid_a, uid_b, same, confidence, reason, decided_at)
  VALUES (?, ?, ?, ?, ?, datetime('now'))
`);

const stmtPairExists = db.prepare(`
  SELECT 1 FROM llm_station_merges WHERE uid_a = ? AND uid_b = ?
`);

export async function enrichStationDedup(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const pairsProvider = opts.pairsProvider || defaultPairs;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;

  let decided = 0;
  let attempted = 0; // counts every LLM call, regardless of parse success.
  // Bound the loop on attempts not on `decided` so a model that returns
  // unparseable JSON can't drag us through all 500 candidate pairs in one tick.
  const pairs = pairsProvider();
  for (const p of pairs) {
    if (attempted >= batchSize) break;
    if (stmtPairExists.get(p.uid_a, p.uid_b)) continue;
    attempted++;
    const { messages, jsonSchema } = buildDedupPairPrompt(p);
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    if (!out || typeof out.same_station !== 'boolean' || typeof out.confidence !== 'number') continue;
    stmtInsertMerge.run(
      p.uid_a, p.uid_b,
      out.same_station ? 1 : 0,
      out.confidence,
      typeof out.reason === 'string' ? out.reason.slice(0, 200) : null,
    );
    decided++;
  }
  return { decided };
}

// Post-cutover: pending social posts live in intel_items under various
// platform-derived source_ids ('misskey-timeline', 'twitter-geo', …).
// Pending = record_type='post' AND lat IS NULL AND geom_source IS NULL.
// post_uid is reconstructed from the master uid suffix.
const stmtPendingSocial = db.prepare(`
  SELECT
    substr(uid, instr(uid, '|') + 1)              AS post_uid,
    COALESCE(sub_source_id, source_id)            AS platform,
    author,
    body                                          AS text,
    title,
    json_extract(properties, '$.media_urls')      AS media_urls
  FROM intel_items
  WHERE record_type = 'post'
    AND lat IS NULL
    AND geom_source IS NULL
    AND (body IS NOT NULL OR title IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

export async function enrichSocialGeocode(opts = {}) {
  const { llmChat, gsiSearch, batchSize } = opts;
  return drainTextRows({
    rowsStmt: stmtPendingSocial,
    buildPrompt: (row) => buildSocialGeocodePrompt({
      platform: row.platform, author: row.author, text: row.text, title: row.title,
      imageUrls: parseJsonArray(row.media_urls), vision: VISION,
    }),
    onOk: (row, place, hit) => applySocialGeocodeOk({
      post_uid: row.post_uid, lat: hit.lat, lon: hit.lon, llm_place_name: place,
    }),
    onFail: (row, sentinel, place) => applySocialGeocodeFail({
      post_uid: row.post_uid, sentinel, llm_place_name: place,
    }),
    llmChat, gsiSearch, batchSize,
  });
}

// Post-cutover: pending cameras = intel_items rows with source_id =
// 'camera-discovery', properties.location_uncertain = 1, and no LLM-resolved
// geom yet. camera_uid is the uid suffix.
const stmtPendingCameras = db.prepare(`
  SELECT
    substr(uid, instr(uid, '|') + 1)              AS camera_uid,
    title                                         AS name,
    lat, lon,
    properties
  FROM intel_items
  WHERE source_id = 'camera-discovery'
    AND geom_source IS NULL OR geom_source = 'native'
    AND json_extract(properties, '$.location_uncertain') = 1
  ORDER BY fetched_at DESC
  LIMIT ?
`);

// camera write paths (base + FTS atomic) live in cameraStore.applyGeocodeOk /
// applyGeocodeFail. cameras has no llm_failure column by design (Task 3 schema
// decision — only llm_place_name + llm_geocoded_at). Failure modes collapse to
// "geocoded_at set, lat unchanged"; clear llm_geocoded_at to re-queue.

// Normalise the LLM's structured output to a clean ordered list of queries.
// Returns null when the response is unparseable, [] when the LLM correctly
// reported "no place inferable".
function extractQueries(out) {
  if (!out || !Array.isArray(out.queries)) return null;
  const cleaned = [];
  for (const q of out.queries) {
    if (typeof q !== 'string') continue;
    const trimmed = q.trim();
    if (!trimmed) continue;
    if (cleaned.includes(trimmed)) continue;
    cleaned.push(trimmed);
    if (cleaned.length >= 3) break;
  }
  return cleaned;
}

// Try queries in order; first GSI hit wins. Returns { hit, query } when a hit
// lands (so the caller can record which level resolved), or { hit: null,
// query: queries[0] } when every fallback missed (records the most-specific
// attempted query for audit / re-queue).
async function resolveViaQueries(queries, gsiSearch) {
  for (const q of queries) {
    const hit = await gsiSearch(q);
    if (hit) return { hit, query: q };
  }
  return { hit: null, query: queries[0] };
}

export async function enrichCameras(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const gsiSearch = opts.gsiSearch || defaultGsi;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;

  const rows = stmtPendingCameras.all(batchSize);
  let geocoded = 0;
  for (const row of rows) {
    const { messages, jsonSchema } = buildSocialGeocodePrompt({
      platform: 'camera',
      author: '',
      text: row.name,
      title: null,
      imageUrls: [],
      vision: false,
    });
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    const queries = extractQueries(out);
    if (queries === null) {
      applyCameraGeocodeFail({ camera_uid: row.camera_uid, llm_place_name: null });
      continue;
    }
    if (queries.length === 0) {
      applyCameraGeocodeFail({ camera_uid: row.camera_uid, llm_place_name: null });
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      applyCameraGeocodeFail({ camera_uid: row.camera_uid, llm_place_name: queries[0] });
      continue;
    }
    const { hit, query } = await resolveViaQueries(queries, gsiSearch);
    if (!hit) {
      applyCameraGeocodeFail({ camera_uid: row.camera_uid, llm_place_name: queries[0] });
      continue;
    }
    const props = safeJson(row.properties);
    props.original_lat = row.lat;
    props.original_lon = row.lon;
    applyCameraGeocodeOk({
      camera_uid: row.camera_uid,
      lat: hit.lat,
      lon: hit.lon,
      llm_place_name: query,
      propertiesJson: JSON.stringify(props),
    });
    geocoded++;
  }
  return { geocoded };
}

async function drainTextRows({ rowsStmt, buildPrompt, onOk, onFail, llmChat, gsiSearch, batchSize }) {
  const _llmChat = llmChat || defaultChat;
  const _gsiSearch = gsiSearch || defaultGsi;
  const _batchSize = batchSize ?? DEFAULT_BATCH;
  const rows = rowsStmt.all(_batchSize);
  let geocoded = 0;
  for (const row of rows) {
    const { messages, jsonSchema } = buildPrompt(row);
    // Route vision-bearing prompts to LLM_VISION_MODEL when configured, else
    // fall through to the default model from llmClient (LLM_MODEL).
    const model = (VISION_MODEL && promptUsesVision(messages)) ? VISION_MODEL : undefined;
    const out = await _llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS, model });
    const queries = extractQueries(out);
    if (queries === null) {
      await onFail(row, '__bad_json__', null);
      continue;
    }
    if (queries.length === 0) {
      await onFail(row, '__no_match__', null);
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      await onFail(row, '__no_match__', queries[0]);
      continue;
    }
    const { hit, query } = await resolveViaQueries(queries, _gsiSearch);
    if (!hit) {
      await onFail(row, '__gsi_miss__', queries[0]);
      continue;
    }
    await onOk(row, query, hit);
    geocoded++;
  }
  return { geocoded };
}

function parseJsonArray(s) {
  if (!s) return [];
  try { const v = JSON.parse(s); return Array.isArray(v) ? v : []; } catch { return []; }
}

function safeJson(s) {
  try { return JSON.parse(s); } catch { return {}; }
}

// ── Intel keyword enrichment ──────────────────────────────────────────────
// Walks intel_items rows whose `keywords` is NULL and asks the local LLM
// (LM Studio + Gemma) for 5–15 search keywords each. On success, writes the
// JSON array to intel_items.keywords AND updates the segmented mirror in
// intel_items_fts.keywords so the next FTS MATCH hits the new tokens. On
// failure, increments keywords_failed; when that counter reaches 5 the row
// is permanently skipped (still searchable via title/body/summary).

const stmtPendingIntel = db.prepare(`
  SELECT uid, source_id, title, body, summary, language
  FROM intel_items
  WHERE keywords IS NULL
    AND COALESCE(keywords_failed, 0) < 5
  ORDER BY fetched_at DESC
  LIMIT ?
`);

// intel base+FTS atomic write goes through intelStore.updateItemKeywords;
// the failure-counter bump is the only UPDATE we issue inline.
const stmtUpdateIntelFail = db.prepare(`
  UPDATE intel_items
  SET keywords_failed = COALESCE(keywords_failed, 0) + 1
  WHERE uid = ?
`);

function sanitizeKeywords(arr) {
  if (!Array.isArray(arr)) return [];
  const cleaned = arr
    .filter((k) => typeof k === 'string')
    .map((k) => k.trim())
    .filter(Boolean);
  return [...new Set(cleaned)].slice(0, 20);
}

export async function enrichIntelKeywords(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;
  const rows = stmtPendingIntel.all(batchSize);

  let enriched = 0;
  let failed = 0;
  for (const row of rows) {
    const { messages, jsonSchema } = buildIntelKeywordsPrompt({
      title: row.title,
      body: row.body,
      summary: row.summary,
      language: row.language,
      source_id: row.source_id,
    });
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    const cleaned = out && Array.isArray(out.keywords) ? sanitizeKeywords(out.keywords) : null;
    if (!cleaned || cleaned.length === 0) {
      stmtUpdateIntelFail.run(row.uid);
      failed += 1;
      continue;
    }

    // intelStore.updateItemKeywords handles JSON encoding for the base column,
    // kuromoji segmentation for the FTS column, and the atomic dual-write txn.
    await updateItemKeywords(row.uid, cleaned);
    enriched += 1;
  }
  return { attempted: rows.length, enriched, failed };
}

// Geocode pass for intel_items rows without coords. Mirrors enrichCameras:
// LLM extracts place queries from title/body/summary, GSI resolves to lat/lon,
// we write back with geom_source='llm'. After 5 failed attempts we sentinel
// geom_source='failed' so we stop retrying — clear that column to re-queue.
const stmtPendingIntelGeo = db.prepare(`
  SELECT uid, source_id, title, body, summary, language
  FROM intel_items
  WHERE lat IS NULL
    AND geom_source IS NULL
    AND COALESCE(geom_failed, 0) < 5
    AND (title IS NOT NULL OR body IS NOT NULL OR summary IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

const stmtIntelGeoOk = db.prepare(`
  UPDATE intel_items
     SET lat = ?, lon = ?,
         geom_source = 'llm',
         geom_at = datetime('now'),
         geom_failed = 0
   WHERE uid = ?
`);

const stmtIntelGeoFail = db.prepare(`
  UPDATE intel_items
     SET geom_failed = COALESCE(geom_failed, 0) + 1,
         geom_source = CASE WHEN COALESCE(geom_failed, 0) + 1 >= 5 THEN 'failed' ELSE geom_source END
   WHERE uid = ?
`);

export async function enrichIntelGeocode(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const gsiSearch = opts.gsiSearch || defaultGsi;
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;

  const rows = stmtPendingIntelGeo.all(batchSize);
  let geocoded = 0;
  let failed = 0;
  for (const row of rows) {
    // Re-use the social-geocode prompt — its job is to pick place names out
    // of arbitrary text, which is exactly the intel case. Vision is off; intel
    // rows don't carry image URLs.
    const text = [row.title, row.summary, row.body].filter(Boolean).join('\n\n');
    if (!text.trim()) {
      stmtIntelGeoFail.run(row.uid);
      failed += 1;
      continue;
    }
    const { messages, jsonSchema } = buildSocialGeocodePrompt({
      platform: 'intel',
      author: '',
      text,
      title: row.title,
      imageUrls: [],
      vision: false,
    });
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    const queries = extractQueries(out);
    if (!queries || queries.length === 0) {
      stmtIntelGeoFail.run(row.uid);
      failed += 1;
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      stmtIntelGeoFail.run(row.uid);
      failed += 1;
      continue;
    }
    const { hit } = await resolveViaQueries(queries, gsiSearch);
    if (!hit) {
      stmtIntelGeoFail.run(row.uid);
      failed += 1;
      continue;
    }
    stmtIntelGeoOk.run(hit.lat, hit.lon, row.uid);
    geocoded += 1;
  }
  return { attempted: rows.length, geocoded, failed };
}

export async function runLlmEnricher() {
  if (process.env.LLM_ENABLED !== 'true') return { skipped: true };
  const out = { skipped: false };
  for (const [name, fn] of [
    ['stationDedup', enrichStationDedup],
    ['social', enrichSocialGeocode],
    ['cameras', enrichCameras],
    ['intelGeo', enrichIntelGeocode],
    ['intelKeywords', enrichIntelKeywords],
  ]) {
    try {
      out[name] = await fn();
    } catch (err) {
      console.warn(`[llmEnricher] ${name} failed:`, err?.message);
      out[name] = { error: err?.message || String(err) };
    }
  }
  return out;
}
