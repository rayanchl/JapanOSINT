// server/src/utils/llmPrompts.js
// Maximum length passed to clip(). Clipped output is n+1 chars (trailing
// ellipsis), so this is a soft ceiling on the LLM's input budget per field.
const CLIP_AT_CHARS = 500;
const MAX_IMAGES = 2;

// queries: ordered list (most-specific first) of Japanese place strings the
// GSI address-search API can resolve. Empty list = "no place inferable" — the
// caller treats that the same as the old `place: null` sentinel.
const PLACE_SCHEMA = {
  type: 'object',
  properties: {
    queries: {
      type: 'array',
      items: { type: 'string', maxLength: 100 },
      minItems: 0,
      maxItems: 3,
    },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
  },
  required: ['queries', 'confidence'],
};

const DEDUP_SCHEMA = {
  type: 'object',
  properties: {
    same_station: { type: 'boolean' },
    confidence:   { type: 'number', minimum: 0, maximum: 1 },
    reason:       { type: 'string', maxLength: 200 },
  },
  required: ['same_station', 'confidence'],
};

function clip(s, n = CLIP_AT_CHARS) {
  if (!s) return '';
  return s.length <= n ? s : s.slice(0, n) + '…';
}

export function buildDedupPairPrompt(p) {
  const system =
    'You are a Japanese rail station identity matcher. Given two station ' +
    'records that are within 150 metres of each other, decide whether they ' +
    'refer to the same physical station/interchange. Two records describe ' +
    'the same station if a passenger could transfer between them without ' +
    'leaving a paid area or by walking through a connected concourse. ' +
    'Different stations on overlapping platforms are NOT the same station.';
  const user =
    `Station A:\n` +
    `  name: ${p.name_a ?? ''}\n` +
    `  name_ja: ${p.name_ja_a ?? ''}\n` +
    `  operator: ${p.operator_a ?? ''}\n` +
    `  line: ${p.line_a ?? ''}\n` +
    `  mode: ${p.mode_a ?? ''}\n\n` +
    `Station B:\n` +
    `  name: ${p.name_b ?? ''}\n` +
    `  name_ja: ${p.name_ja_b ?? ''}\n` +
    `  operator: ${p.operator_b ?? ''}\n` +
    `  line: ${p.line_b ?? ''}\n` +
    `  mode: ${p.mode_b ?? ''}\n\n` +
    `Distance: ${Math.round(p.dist_m ?? 0)} m`;
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: user },
    ],
    jsonSchema: DEDUP_SCHEMA,
  };
}

export function buildSocialGeocodePrompt(p) {
  const system =
    'You extract Japanese place references from social media posts and turn ' +
    'them into queries the GSI Japanese address-search API can resolve. ' +
    'Return up to 3 queries ordered from most specific to broadest. Prefer ' +
    'Japanese (kanji + kana) over romaji — GSI matches Japanese far better. ' +
    'When you can confidently infer an administrative address, format the ' +
    'most specific query as `<prefecture><city><ward><district>` (e.g. ' +
    '`東京都渋谷区道玄坂`); when only a landmark or station is mentioned, ' +
    'use that name (e.g. `東京タワー`, `渋谷駅`). Always include at least one ' +
    'broader fallback (city or prefecture) as the last entry. Prefer the ' +
    'place where the author is, not places merely mentioned in conversation. ' +
    'If no place can be inferred, return an empty queries list.';
  const userText =
    `Platform: ${p.platform}\n` +
    `Author: ${p.author ?? ''}\n` +
    `Title: ${clip(p.title) || '(none)'}\n` +
    `Text: ${clip(p.text) || '(none)'}`;
  const images = (p.vision && Array.isArray(p.imageUrls))
    ? p.imageUrls.slice(0, MAX_IMAGES).map((url) => ({ type: 'image_url', image_url: { url } }))
    : [];
  const userContent = images.length === 0
    ? userText
    : [{ type: 'text', text: userText }, ...images];
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: userContent },
    ],
    jsonSchema: PLACE_SCHEMA,
  };
}

// ── Intel keyword extraction ──────────────────────────────────────────────
// Used by enrichIntelKeywords(): an LM-Studio-hosted local model (Gemma)
// reads a non-spatial intel item (RSS article, advisory, doc index entry…)
// and returns a small list of search keywords. Output is dual-language when
// applicable so a JA-only article is searchable by the English equivalent
// term, and vice versa.

const INTEL_KEYWORDS_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['keywords'],
  properties: {
    keywords: {
      type: 'array',
      minItems: 0,
      maxItems: 20,
      items: { type: 'string', minLength: 1, maxLength: 80 },
    },
  },
};

const INTEL_KEYWORDS_BODY_CLIP = 4000;

export function buildIntelKeywordsPrompt({ title, body, summary, language, source_id }) {
  const text = [title, summary || body]
    .filter(Boolean)
    .join('\n\n')
    .slice(0, INTEL_KEYWORDS_BODY_CLIP);
  const system =
    'You extract search keywords for a Japan-focused OSINT index. Given a ' +
    'title and body, return 5–15 keywords that would let someone find this ' +
    'item later via full-text search. INCLUDE: proper nouns (people, places, ' +
    'organisations, product names), key topical nouns. For Japanese sources ' +
    'output BOTH the original Japanese AND a romaji or English equivalent ' +
    'when one exists (e.g. include both "東京" and "Tokyo", "気象庁" and ' +
    '"JMA"). Avoid generic stopwords. Output JSON only.';
  const user =
    `Source: ${source_id || '(unknown)'}\n` +
    `Language: ${language || 'auto'}\n\n` +
    text;
  return {
    messages: [
      { role: 'system', content: system },
      { role: 'user', content: user },
    ],
    jsonSchema: INTEL_KEYWORDS_SCHEMA,
  };
}
