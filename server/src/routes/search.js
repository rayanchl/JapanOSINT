/**
 * Interactive OSINT search (ported OSINTsaas pipeline).
 *
 *   POST /api/search/analyze        { query, max_rounds? } -> { request_id }
 *   GET  /api/search/suggest?q=     -> { suggestions: [...9] }
 *   GET  /api/search/results/:id    -> final snapshot (from memory or store)
 *   GET  /api/search/stream/:id     -> SSE progress (registered pre-auth in
 *                                      index.js: EventSource can't send the
 *                                      Authorization header; the unguessable
 *                                      request_id UUID is the capability)
 *
 * Ingestion runs server-side off the pipeline 'done' event, independent of
 * the client SSE — a disconnected client never loses results.
 */

import express from 'express';
import { runPipeline, getSuggestions, getRequest } from '../osint/pipeline.js';
import { ingestSearchRun } from '../utils/searchIngest.js';
import { getItemByUid } from '../utils/intelStore.js';
import { withTenantScope } from '../utils/collectorTap.js';

const router = express.Router();

router.post('/analyze', (req, res) => {
  const query = (req.body?.query ?? '').toString().trim();
  if (!query) return res.status(400).json({ error: 'query_required' });
  const maxRounds = Number.isFinite(req.body?.max_rounds) ? req.body.max_rounds : undefined;
  // Run inside the caller's tenant scope: the pipeline is fire-and-forget, so
  // wrapping the synchronous runPipeline() call propagates the ALS context to
  // every async round/service. Any envFor() in the pipeline now resolves the
  // tenant's BYOK key, and consumed credentials are metered under
  // 'osint-search'. tap-silent → no Follow-log noise.
  const rp = withTenantScope(req.tenant?.id, 'osint-search', () =>
    runPipeline(query, { maxRounds }),
  );
  rp.once('done', (snap) => {
    ingestSearchRun(snap).catch((e) => console.warn('[search] ingest failed:', e?.message));
  });
  res.json({ request_id: rp.request_id, status: 'processing', query });
});

router.get('/suggest', async (req, res) => {
  const q = (req.query?.q ?? '').toString().trim();
  if (!q) return res.json({ suggestions: [] });
  try {
    res.json({ suggestions: await getSuggestions(q) });
  } catch {
    res.json({ suggestions: [] });
  }
});

router.get('/results/:id', (req, res) => {
  const rp = getRequest(req.params.id);
  if (rp) return res.json(rp.toJSON());
  // Server restarted: reconstruct from the persisted run row.
  const row = getItemByUid(`osint-search|run:${req.params.id}`);
  if (!row) return res.status(404).json({ error: 'not_found' });
  let props = {};
  try { props = JSON.parse(row.properties || '{}'); } catch { /* noop */ }
  res.json({
    request_id: req.params.id, query: props.query || row.title,
    phase: props.phase || 'completed', progress_percent: 100,
    entities: props.entities || [], stats: props.stats || {},
    results: { synthesis: row.summary }, from_store: true,
  });
});

/**
 * SSE handler — registered BEFORE the /api auth middleware in index.js.
 * Streams progressTracker snapshots; compression is exempted globally for
 * text/event-stream (see index.js sseCompressionFilter).
 */
export function searchStreamHandler(req, res) {
  const rp = getRequest(req.params.id);
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache, no-transform');
  res.setHeader('Connection', 'keep-alive');
  res.setHeader('X-Accel-Buffering', 'no');
  res.flushHeaders?.();

  if (!rp) {
    res.write(`event: error\ndata: ${JSON.stringify({ error: 'not_found' })}\n\n`);
    return res.end();
  }

  const send = (snap) => {
    res.write(`event: progress\ndata: ${JSON.stringify(snap)}\n\n`);
  };
  send(rp.toJSON()); // initial snapshot
  if (rp.done) {
    res.write(`event: close\ndata: {}\n\n`);
    return res.end();
  }
  const onUpdate = (snap) => send(snap);
  const onDone = (snap) => {
    send(snap);
    res.write(`event: close\ndata: {}\n\n`);
    cleanup();
    res.end();
  };
  const keepalive = setInterval(() => res.write(': ping\n\n'), 15000);
  function cleanup() {
    clearInterval(keepalive);
    rp.off('update', onUpdate);
    rp.off('done', onDone);
  }
  rp.on('update', onUpdate);
  rp.on('done', onDone);
  req.on('close', cleanup);
}

export default router;
