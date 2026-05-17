/**
 * Entity-centric investigation API. Reads the unified entity graph (collector
 * NER + OSINT-search discoveries share one store), so these work even when
 * the LLM/pipeline are unavailable — pure SQLite.
 *
 *   GET /api/entities/search?q=&type=&limit=        FTS over entities
 *   GET /api/entities/:type/:id                     entity profile
 *   GET /api/entities/:type/:id/graph?depth=1..3    relationship ego-graph
 *   GET /api/entities/:type/:id/mentions?limit=&offset=
 */

import express from 'express';
import db from '../utils/database.js';
import { searchEntities, getEntity, getGraph, listMentions } from '../utils/entityStore.js';

const router = express.Router();

// Backfill / health monitor: NER coverage of the corpus + graph size.
router.get('/stats', (_req, res) => {
  const n = (sql) => db.prepare(sql).get().n;
  res.json({
    intel_items: n('SELECT COUNT(*) n FROM intel_items'),
    extracted: n("SELECT COUNT(*) n FROM entity_extraction_state WHERE extracted_at != ''"),
    failed: n('SELECT COUNT(*) n FROM entity_extraction_state WHERE failed_count >= 5'),
    entities: n('SELECT COUNT(*) n FROM entities'),
    mentions: n('SELECT COUNT(*) n FROM entity_mentions'),
    relationships: n('SELECT COUNT(*) n FROM entity_relationships'),
  });
});

router.get('/search', async (req, res) => {
  const q = (req.query?.q ?? '').toString().trim();
  if (!q) return res.json({ results: [] });
  const type = req.query?.type ? String(req.query.type) : null;
  const limit = Math.min(Number(req.query?.limit) || 30, 100);
  try {
    res.json({ results: await searchEntities({ q, type, limit }) });
  } catch (e) {
    res.status(500).json({ error: 'search_failed', detail: e?.message });
  }
});

router.get('/:type/:id', (req, res) => {
  const e = getEntity(req.params.id);
  if (!e || e.type !== req.params.type) return res.status(404).json({ error: 'not_found' });
  let aliases = [];
  let properties = {};
  try { aliases = JSON.parse(e.aliases_json || '[]'); } catch { /* noop */ }
  try { properties = JSON.parse(e.properties || '{}'); } catch { /* noop */ }
  res.json({
    entity_id: e.entity_id, type: e.type, value: e.canonical,
    name_ja: e.name_ja, name_romaji: e.name_romaji, aliases, properties,
    mention_count: e.mention_count, first_seen_at: e.first_seen_at, last_seen_at: e.last_seen_at,
  });
});

router.get('/:type/:id/graph', (req, res) => {
  const e = getEntity(req.params.id);
  if (!e || e.type !== req.params.type) return res.status(404).json({ error: 'not_found' });
  const depth = Math.min(Math.max(Number(req.query?.depth) || 1, 1), 3);
  res.json(getGraph(req.params.id, { depth }));
});

router.get('/:type/:id/mentions', (req, res) => {
  const e = getEntity(req.params.id);
  if (!e || e.type !== req.params.type) return res.status(404).json({ error: 'not_found' });
  const limit = Math.min(Number(req.query?.limit) || 50, 200);
  const offset = Math.max(Number(req.query?.offset) || 0, 0);
  res.json({ mentions: listMentions(req.params.id, { limit, offset }) });
});

export default router;
