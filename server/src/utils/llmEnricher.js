// server/src/utils/llmEnricher.js
import db from './database.js';
import { chat as defaultChat } from './llmClient.js';
import { gsiAddressSearch as defaultGsi } from './gsiAddressSearch.js';
import { findUncertainStationPairs as defaultPairs } from './stationClusterer.js';
import {
  buildDedupPairPrompt,
  buildSocialGeocodePrompt,
  buildVideoGeocodePrompt,
} from './llmPrompts.js';

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

const stmtPendingSocial = db.prepare(`
  SELECT post_uid, platform, author, text, title, media_urls
  FROM social_posts
  WHERE llm_geocoded_at IS NULL
    AND (text IS NOT NULL OR title IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

const stmtUpdateSocialOk = db.prepare(`
  UPDATE social_posts
  SET lat = ?, lon = ?, geo_source = 'llm_gsi',
      llm_place_name = ?, llm_geocoded_at = datetime('now'), llm_failure = NULL
  WHERE post_uid = ?
`);

const stmtUpdateSocialFail = db.prepare(`
  UPDATE social_posts
  SET llm_geocoded_at = datetime('now'), llm_failure = ?, llm_place_name = ?
  WHERE post_uid = ?
`);

export async function enrichSocialGeocode(opts = {}) {
  const { llmChat, gsiSearch, batchSize } = opts;
  return drainTextRows({
    rowsStmt: stmtPendingSocial,
    buildPrompt: (row) => buildSocialGeocodePrompt({
      platform: row.platform, author: row.author, text: row.text, title: row.title,
      imageUrls: parseJsonArray(row.media_urls), vision: VISION,
    }),
    onOk: (row, place, hit) => stmtUpdateSocialOk.run(hit.lat, hit.lon, place, row.post_uid),
    onFail: (row, sentinel, place) => stmtUpdateSocialFail.run(sentinel, place, row.post_uid),
    llmChat, gsiSearch, batchSize,
  });
}

const stmtPendingVideo = db.prepare(`
  SELECT video_uid, platform, channel, title, description, thumbnail_url
  FROM video_items
  WHERE llm_geocoded_at IS NULL
    AND (title IS NOT NULL OR description IS NOT NULL)
  ORDER BY fetched_at DESC
  LIMIT ?
`);

const stmtUpdateVideoOk = db.prepare(`
  UPDATE video_items
  SET lat = ?, lon = ?, geo_source = 'llm_gsi',
      llm_place_name = ?, llm_geocoded_at = datetime('now'), llm_failure = NULL
  WHERE video_uid = ?
`);

const stmtUpdateVideoFail = db.prepare(`
  UPDATE video_items
  SET llm_geocoded_at = datetime('now'), llm_failure = ?, llm_place_name = ?
  WHERE video_uid = ?
`);

export async function enrichVideoGeocode(opts = {}) {
  const { llmChat, gsiSearch, batchSize } = opts;
  return drainTextRows({
    rowsStmt: stmtPendingVideo,
    buildPrompt: (row) => buildVideoGeocodePrompt({
      platform: row.platform, channel: row.channel, title: row.title,
      description: row.description, thumbnailUrl: row.thumbnail_url, vision: VISION,
    }),
    onOk: (row, place, hit) => stmtUpdateVideoOk.run(hit.lat, hit.lon, place, row.video_uid),
    onFail: (row, sentinel, place) => stmtUpdateVideoFail.run(sentinel, place, row.video_uid),
    llmChat, gsiSearch, batchSize,
  });
}

const stmtPendingCameras = db.prepare(`
  SELECT camera_uid, name, lat, lon, properties
  FROM cameras
  WHERE llm_geocoded_at IS NULL
    AND json_extract(properties, '$.location_uncertain') = 1
  ORDER BY last_seen_at DESC
  LIMIT ?
`);

const stmtUpdateCameraOk = db.prepare(`
  UPDATE cameras
  SET lat = ?, lon = ?, llm_place_name = ?,
      llm_geocoded_at = datetime('now'),
      properties = ?
  WHERE camera_uid = ?
`);

// cameras has no llm_failure column by design (Task 3 schema decision —
// cameras only carry llm_place_name + llm_geocoded_at). Failure modes
// collapse to "geocoded_at set, lat unchanged"; the operator can re-queue
// by clearing llm_geocoded_at.
const stmtUpdateCameraFail = db.prepare(`
  UPDATE cameras
  SET llm_geocoded_at = datetime('now'), llm_place_name = ?
  WHERE camera_uid = ?
`);

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
      stmtUpdateCameraFail.run(null, row.camera_uid);
      continue;
    }
    if (queries.length === 0) {
      stmtUpdateCameraFail.run(null, row.camera_uid);
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      stmtUpdateCameraFail.run(queries[0], row.camera_uid);
      continue;
    }
    const { hit, query } = await resolveViaQueries(queries, gsiSearch);
    if (!hit) {
      stmtUpdateCameraFail.run(queries[0], row.camera_uid);
      continue;
    }
    const props = safeJson(row.properties);
    props.original_lat = row.lat;
    props.original_lon = row.lon;
    stmtUpdateCameraOk.run(hit.lat, hit.lon, query, JSON.stringify(props), row.camera_uid);
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
      onFail(row, '__bad_json__', null);
      continue;
    }
    if (queries.length === 0) {
      onFail(row, '__no_match__', null);
      continue;
    }
    if (typeof out.confidence === 'number' && out.confidence < PLACE_CONFIDENCE_GATE) {
      onFail(row, '__no_match__', queries[0]);
      continue;
    }
    const { hit, query } = await resolveViaQueries(queries, _gsiSearch);
    if (!hit) {
      onFail(row, '__gsi_miss__', queries[0]);
      continue;
    }
    onOk(row, query, hit);
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

export async function runLlmEnricher() {
  if (process.env.LLM_ENABLED !== 'true') return { skipped: true };
  const out = { skipped: false };
  for (const [name, fn] of [
    ['stationDedup', enrichStationDedup],
    ['social', enrichSocialGeocode],
    ['video', enrichVideoGeocode],
    ['cameras', enrichCameras],
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
