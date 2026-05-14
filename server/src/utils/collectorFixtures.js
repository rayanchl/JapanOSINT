import { mkdirSync, writeFileSync, readFileSync, existsSync, statSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import { createHash } from 'crypto';

const __dirname = dirname(fileURLToPath(import.meta.url));
const FIXTURES_ROOT = resolve(__dirname, '../../data/collector-fixtures');

mkdirSync(FIXTURES_ROOT, { recursive: true });

const EXT_BY_CONTENT_TYPE = [
  [/^application\/(?:json|geo\+json|ld\+json|vnd\.api\+json)/i, 'json'],
  [/^application\/xml|^text\/xml|\+xml/i,                       'xml'],
  [/^text\/html/i,                                              'html'],
  [/^text\/csv/i,                                               'csv'],
  [/^text\/plain/i,                                             'txt'],
  [/^application\/x-protobuf|^application\/octet-stream/i,      'bin'],
];

function extForContentType(contentType) {
  if (!contentType) return 'txt';
  for (const [re, ext] of EXT_BY_CONTENT_TYPE) {
    if (re.test(contentType)) return ext;
  }
  return 'txt';
}

// sourceId is registry-controlled (kebab-case) but defense-in-depth: refuse
// anything with path separators or leading dots so a malformed id can't
// escape FIXTURES_ROOT.
function safeSourceId(sourceId) {
  if (!sourceId || typeof sourceId !== 'string') {
    throw new Error('collectorFixtures: sourceId must be a non-empty string');
  }
  if (sourceId.includes('/') || sourceId.includes('\\') || sourceId.startsWith('.')) {
    throw new Error(`collectorFixtures: invalid sourceId "${sourceId}"`);
  }
  return sourceId;
}

function fixtureDir(sourceId) {
  return resolve(FIXTURES_ROOT, safeSourceId(sourceId));
}

function metaPath(sourceId) {
  return resolve(fixtureDir(sourceId), 'meta.json');
}

export function hasFixture(sourceId) {
  return existsSync(metaPath(sourceId));
}

export function getFixtureMeta(sourceId) {
  const p = metaPath(sourceId);
  if (!existsSync(p)) return null;
  try {
    return JSON.parse(readFileSync(p, 'utf8'));
  } catch {
    return null;
  }
}

export function loadFixture(sourceId) {
  const meta = getFixtureMeta(sourceId);
  if (!meta) return null;
  const rawPath = resolve(fixtureDir(sourceId), meta.raw_filename);
  if (!existsSync(rawPath)) return null;
  const isBinary = meta.raw_filename.endsWith('.bin');
  return {
    meta,
    raw: readFileSync(rawPath, isBinary ? null : 'utf8'),
  };
}

/**
 * Persist a raw fetch payload as the golden fixture for a source. Phase 0
 * writes one fixture per source on the first healthy run; Phase 3 will
 * re-capture after a verified fix. Callers must have already validated
 * that the run is healthy (status=online, records_count > 0).
 */
export function captureFixture({
  sourceId,
  rawBody,
  contentType = null,
  statusCode = null,
  recordsCount = null,
  sourceUrl = null,
}) {
  const dir = fixtureDir(sourceId);
  mkdirSync(dir, { recursive: true });

  const ext = extForContentType(contentType);
  const rawFilename = `raw.${ext}`;
  const rawPath = resolve(dir, rawFilename);
  const body = typeof rawBody === 'string' ? rawBody : Buffer.from(rawBody || '');
  writeFileSync(rawPath, body);

  const meta = {
    source_id: sourceId,
    source_url: sourceUrl,
    captured_at: new Date().toISOString(),
    content_type: contentType,
    status_code: statusCode,
    records_count: recordsCount,
    raw_filename: rawFilename,
    raw_bytes: typeof body === 'string' ? Buffer.byteLength(body) : body.length,
    raw_sha256: createHash('sha256')
      .update(typeof body === 'string' ? body : body)
      .digest('hex'),
  };
  writeFileSync(metaPath(sourceId), JSON.stringify(meta, null, 2));
  return meta;
}

export function fixtureAge(sourceId) {
  const p = metaPath(sourceId);
  if (!existsSync(p)) return null;
  return Date.now() - statSync(p).mtimeMs;
}
