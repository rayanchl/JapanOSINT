// server/src/utils/llmPrompts.js
const MAX_TEXT_CHARS = 500;
const MAX_IMAGES = 2;

const PLACE_SCHEMA = {
  type: 'object',
  properties: {
    place:      { type: ['string', 'null'], maxLength: 100 },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
  },
  required: ['place', 'confidence'],
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

function clip(s, n = MAX_TEXT_CHARS) {
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
    `  name: ${JSON.stringify(p.name_a ?? '')}\n` +
    `  name_ja: ${JSON.stringify(p.name_ja_a ?? '')}\n` +
    `  operator: ${JSON.stringify(p.operator_a ?? '')}\n` +
    `  line: ${JSON.stringify(p.line_a ?? '')}\n` +
    `  mode: ${p.mode_a ?? ''}\n\n` +
    `Station B:\n` +
    `  name: ${JSON.stringify(p.name_b ?? '')}\n` +
    `  name_ja: ${JSON.stringify(p.name_ja_b ?? '')}\n` +
    `  operator: ${JSON.stringify(p.operator_b ?? '')}\n` +
    `  line: ${JSON.stringify(p.line_b ?? '')}\n` +
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
    'You extract Japanese place names from social media posts. Return the ' +
    'single most specific Japanese place mentioned in the post — a ' +
    'neighbourhood, station, landmark, or address. Prefer the place where ' +
    'the author is, not places merely mentioned in conversation. If no ' +
    'place is mentioned, return null.';
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

export function buildVideoGeocodePrompt(p) {
  const system =
    'You extract Japanese place names from video metadata. Return the ' +
    'single most specific Japanese place where the video was filmed or ' +
    'that the video is about — a neighbourhood, station, landmark, or ' +
    'address. If no place can be inferred, return null.';
  const userText =
    `Platform: ${p.platform}\n` +
    `Channel: ${p.channel ?? ''}\n` +
    `Title: ${clip(p.title) || '(none)'}\n` +
    `Description: ${clip(p.description) || '(none)'}`;
  const images = (p.vision && p.thumbnailUrl)
    ? [{ type: 'image_url', image_url: { url: p.thumbnailUrl } }]
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
