/**
 * Run the camera-discovery fan-out end-to-end:
 *   1. Call collectCameraDiscovery with per-feature / per-channel callbacks.
 *   2. Upsert every emitted feature into the persistent `cameras` table.
 *   3. Broadcast WebSocket events so the UI can render a live discovery thread.
 *
 * One run can take minutes (Overpass tiled, Chromium scrapers). The caller
 * passes the WebSocketServer so broadcasts reach every connected client.
 *
 * A 10-minute hard ceiling prevents a pathologically stuck channel from
 * blocking the next scheduled run.
 */

import collectCameraDiscovery from '../collectors/cameraDiscovery.js';
import { upsertCamera, cameraStats } from './cameraStore.js';

const RUN_CEILING_MS = 10 * 60 * 1000;

let _inflightRun = null;
let _lastRunAt = null; // ISO timestamp of the most recent completed run

function broadcast(wsServer, payload) {
  if (!wsServer) return;
  const msg = JSON.stringify(payload);
  for (const client of wsServer.clients) {
    if (client.readyState === 1) {
      try { client.send(msg); } catch {}
    }
  }
}

export async function runCameraDiscovery(wsServer) {
  if (_inflightRun) {
    // Don't stack runs — the scheduler could fire while the previous run is
    // still crunching through Overpass.
    return _inflightRun;
  }

  const run_id = new Date().toISOString();
  const started = Date.now();
  const runCounts = {};
  let newCount = 0;
  let updatedCount = 0;

  console.log(`[cameraRunner] starting run ${run_id}`);
  broadcast(wsServer, { type: 'camera_run_start', run_id });

  const task = (async () => {
    const ceiling = new Promise((_, rej) =>
      setTimeout(() => rej(new Error(`run ceiling ${RUN_CEILING_MS}ms exceeded`)), RUN_CEILING_MS),
    );

    const work = collectCameraDiscovery({
      onCamera: (feature, channel) => {
        runCounts[channel] = (runCounts[channel] || 0) + 1;
        let result = null;
        try {
          result = upsertCamera(feature, channel);
        } catch (err) {
          console.error('[cameraRunner] upsert failed:', err?.message);
          return;
        }
        if (!result) return;
        if (result.kind === 'new') newCount++; else updatedCount++;
        broadcast(wsServer, {
          type: 'camera_discovered',
          kind: result.kind,
          channel,
          camera: result.camera,
          run_id,
          run_counts: runCounts,
        });
      },
      onChannelDone: (name, summary) => {
        broadcast(wsServer, {
          type: 'camera_channel_done',
          channel: name,
          ok: summary.ok,
          count: summary.count,
          run_id,
        });
      },
    });

    try {
      return await Promise.race([work, ceiling]);
    } catch (err) {
      console.error('[cameraRunner] aborted:', err?.message);
      return null;
    }
  })();

  _inflightRun = task;

  try {
    const result = await task;
    const stats = cameraStats();
    const elapsed = Date.now() - started;
    console.log(
      `[cameraRunner] run ${run_id} done in ${(elapsed / 1000).toFixed(1)}s` +
      ` — ${newCount} new, ${updatedCount} updated; DB total ${stats.total}`,
    );
    broadcast(wsServer, {
      type: 'camera_run_end',
      run_id,
      elapsed_ms: elapsed,
      new_count: newCount,
      updated_count: updatedCount,
      run_counts: runCounts,
      channel_counts: result?._meta?.channel_counts || {},
      channel_errors: result?._meta?.channel_errors || {},
      db_total: stats.total,
      db_new_24h: stats.new24h,
    });
    return result;
  } finally {
    _inflightRun = null;
    _lastRunAt = new Date().toISOString();
  }
}

export function isRunInFlight() {
  return _inflightRun !== null;
}

export function getLastRunAt() {
  return _lastRunAt;
}
