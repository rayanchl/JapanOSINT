/**
 * Broadcast per-layer work indicators over the shared /ws channel.
 *
 * Event shapes:
 *   layer_work_started  { type, layer_id, collector_key, source_id, source_name, timestamp }
 *   layer_work_finished { type, layer_id, collector_key, source_id, source_name,
 *                         duration_ms, record_count, cache_status, timestamp }
 *
 * The client (useLayerLoading.js) listens and drives the per-layer spinner,
 * independent of whether the client's own /api/data/* fetch is in flight.
 */

import { getBroadcaster } from './collectorTap.js';
import sources from './sourceRegistry.js';

const sourceByKey = new Map();
function lookupSource(key) {
  if (sourceByKey.has(key)) return sourceByKey.get(key);
  const s = sources.find((x) => x.id === key) || null;
  sourceByKey.set(key, s);
  return s;
}

function broadcast(payload) {
  const ws = getBroadcaster();
  if (!ws) return;
  let msg;
  try { msg = JSON.stringify(payload); } catch { return; }
  for (const client of ws.clients) {
    if (client.readyState === 1) {
      try { client.send(msg); } catch {}
    }
  }
}

export function broadcastLayerWorkStarted({ layerId, collectorKey, sourceId }) {
  const src = lookupSource(collectorKey);
  broadcast({
    type: 'layer_work_started',
    layer_id: layerId,
    collector_key: collectorKey,
    source_id: sourceId ?? src?.id ?? collectorKey,
    source_name: src?.name ?? collectorKey,
    timestamp: new Date().toISOString(),
  });
}

export function broadcastLayerWorkFinished({
  layerId, collectorKey, sourceId,
  durationMs = 0, recordCount = null, cacheStatus = 'miss',
}) {
  const src = lookupSource(collectorKey);
  broadcast({
    type: 'layer_work_finished',
    layer_id: layerId,
    collector_key: collectorKey,
    source_id: sourceId ?? src?.id ?? collectorKey,
    source_name: src?.name ?? collectorKey,
    duration_ms: durationMs,
    record_count: recordCount,
    cache_status: cacheStatus,   // 'hit' | 'miss' | 'error'
    timestamp: new Date().toISOString(),
  });
}
