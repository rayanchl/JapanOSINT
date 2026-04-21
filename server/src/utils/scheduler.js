import cron from 'node-cron';
import sources from './sourceRegistry.js';
import {
  upsertSource,
  updateSourceStatus,
  logFetch,
  pruneSourcesNotIn,
} from './database.js';
import { redactUrl, redactHeaders, truncateBody } from './redact.js';
import { runCameraDiscovery } from './cameraRunner.js';
import { runTransportDiscovery } from './transportRunner.js';
import { withCollectorRun } from './collectorTap.js';
import { seedTtlsFromRegistry, pruneExpired } from './collectorCache.js';

/**
 * Some upstream endpoints only accept POST (e.g. Overpass `/api/interpreter`
 * returns HTTP 400 on bare GET). Probing those with GET would always mark the
 * source `degraded` even though the collector works fine. Map such URLs to a
 * sibling endpoint that responds to GET with 2xx so the health probe reflects
 * reality. The display URL in the registry stays accurate for humans.
 */
function probeUrlFor(url) {
  if (!url) return url;
  // Overpass: /api/interpreter is POST-only; /api/status is the plain-text
  // health endpoint served by every Overpass mirror.
  if (/^https?:\/\/[^/]*overpass[^/]*\/api\/interpreter(?:[/?#]|$)/i.test(url)) {
    return url.replace(/\/api\/interpreter(?:[?#].*)?$/i, '/api/status');
  }
  return url;
}

/**
 * Attempt a simple HTTP fetch for a source URL, record timing and status.
 * Only runs for free API sources that have a real URL.
 */
async function fetchSource(source, wsServer) {
  const start = Date.now();
  const probeUrl = probeUrlFor(source.url);
  const requestHeaders = {
    accept: '*/*',
    'user-agent': 'JapanOSINT-probe/1.0',
  };
  const redactedReqUrl = redactUrl(probeUrl);
  const redactedReqHeaders = JSON.stringify(redactHeaders(requestHeaders));

  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15_000);

    const res = await fetch(probeUrl, {
      signal: controller.signal,
      headers: requestHeaders,
      redirect: 'follow',
    });
    clearTimeout(timeout);

    const duration = Date.now() - start;
    const ok = res.ok;
    const rawBody = await res.text();

    let recordsCount = 0;
    try {
      const json = JSON.parse(rawBody);
      if (Array.isArray(json)) {
        recordsCount = json.length;
      } else if (json && typeof json === 'object') {
        const data = json.data ?? json.results ?? json.features ?? json.items;
        if (Array.isArray(data)) recordsCount = data.length;
        else recordsCount = 1;
      }
    } catch {
      // Not JSON — leave recordsCount at 0
    }

    const status = ok ? 'online' : 'degraded';
    const responseHeadersJson = JSON.stringify(redactHeaders(res.headers));
    const bodyPreview = truncateBody(rawBody, 200);

    updateSourceStatus({
      id: source.id,
      status,
      response_time_ms: duration,
      records_count: recordsCount || null,
      error_message: ok ? null : `HTTP ${res.status}`,
      probe_request_url: redactedReqUrl,
      probe_request_method: 'GET',
      probe_request_headers: redactedReqHeaders,
      probe_response_status: res.status,
      probe_response_headers: responseHeadersJson,
      probe_response_body: bodyPreview,
      probe_kind: source.type,
    });

    logFetch({
      source_id: source.id,
      status,
      records_fetched: recordsCount,
      duration_ms: duration,
      error: ok ? null : `HTTP ${res.status}`,
    });

    broadcast(wsServer, {
      type: 'source_update',
      source_id: source.id,
      status,
      response_time_ms: duration,
      records_count: recordsCount,
    });
  } catch (err) {
    const duration = Date.now() - start;
    const errorMsg = err.name === 'AbortError' ? 'Timeout (15s)' : err.message;

    updateSourceStatus({
      id: source.id,
      status: 'offline',
      response_time_ms: duration,
      error_message: errorMsg,
      probe_request_url: redactedReqUrl,
      probe_request_method: 'GET',
      probe_request_headers: redactedReqHeaders,
      probe_response_status: null,
      probe_response_headers: null,
      probe_response_body: null,
      probe_kind: source.type,
    });

    logFetch({
      source_id: source.id,
      status: 'offline',
      records_fetched: 0,
      duration_ms: duration,
      error: errorMsg,
    });

    broadcast(wsServer, {
      type: 'source_update',
      source_id: source.id,
      status: 'offline',
      error: errorMsg,
    });
  }
}

/**
 * Send a JSON message to all connected WebSocket clients.
 */
function broadcast(wsServer, data) {
  if (!wsServer) return;
  const msg = JSON.stringify(data);
  for (const client of wsServer.clients) {
    if (client.readyState === 1) {
      client.send(msg);
    }
  }
}

/**
 * Probe cadence for source health checks. We probe every source every 2
 * hours regardless of its `updateInterval` — the per-source interval
 * controls collector data freshness, not health-probe frequency, and a
 * uniform 2h heartbeat is enough to surface outages without burning
 * upstream bandwidth on full GETs.
 */
function intervalToCron(_seconds) {
  return '0 */2 * * *';
}

/**
 * Start the scheduler: seed the database, schedule periodic fetches.
 * @param {import('ws').WebSocketServer} wsServer
 */
export function startScheduler(wsServer) {
  // 1. Register all sources in the database as 'pending' — the real status
  //    is set by the first probe sweep.
  for (const src of sources) {
    upsertSource({
      id: src.id,
      name: src.name,
      type: src.type,
      category: src.category,
      url: src.url,
      status: 'pending',
    });
  }

  const pruned = pruneSourcesNotIn(sources.map((s) => s.id));
  if (pruned.length > 0) {
    console.log(`[scheduler] Pruned ${pruned.length} removed source(s): ${pruned.join(', ')}`);
  }

  console.log(`[scheduler] Registered ${sources.length} sources in database`);

  // 1b. Seed per-source TTLs from sourceRegistry.updateInterval. Idempotent —
  //     runtime edits to collector_ttls survive restarts.
  try {
    const { inserted, total } = seedTtlsFromRegistry();
    console.log(`[scheduler] Seeded collector TTLs — ${inserted} new (of ${total})`);
  } catch (err) {
    console.error('[scheduler] TTL seed failed:', err?.message);
  }

  // 1c. Prune expired collector_cache rows every 10 min.
  cron.schedule('*/10 * * * *', () => {
    try {
      const removed = pruneExpired();
      if (removed > 0) console.log(`[scheduler] Pruned ${removed} expired cache row(s)`);
    } catch (err) {
      console.error('[scheduler] cache prune failed:', err?.message);
    }
  });

  // 2. Schedule periodic probes for every source with a real http(s) URL.
  //    Paid sources without keys simply return 401/403 — that's honest info.
  //    Skip sentinel schemes (e.g. `internal://unified-subways`): those are
  //    placeholders for fused collectors with no upstream to probe.
  const schedulable = sources.filter(
    (s) => typeof s.url === 'string' && /^https?:\/\//i.test(s.url),
  );

  for (const src of schedulable) {
    const cronExpr = intervalToCron(src.updateInterval);
    cron.schedule(cronExpr, () => {
      withCollectorRun(src.id, () => fetchSource(src, wsServer), { trigger: 'cron' })
        .catch((err) => {
          console.error(`[scheduler] Unhandled error fetching ${src.id}:`, err);
        });
    });
  }

  console.log(`[scheduler] Scheduled ${schedulable.length} free API source jobs`);

  // 4. Broadcast a heartbeat every 30 seconds
  setInterval(() => {
    broadcast(wsServer, { type: 'heartbeat', timestamp: new Date().toISOString() });
  }, 30_000);

  // 5. Camera discovery runner — persistent deduplicated DB + live WS stream.
  //    Hourly cron at :15 so it doesn't collide with on-the-hour probes.
  cron.schedule('15 * * * *', () => {
    withCollectorRun('cameraDiscovery', () => runCameraDiscovery(wsServer), { trigger: 'cron' })
      .catch((err) => {
        console.error('[scheduler] camera run failed:', err?.message);
      });
  });

  // 6. Unified transport runner — same hourly cadence as cameras, offset to
  //    :30 so it doesn't collide on the same minute. Persists deduped
  //    stations + tracks to SQLite; the layer endpoints read straight
  //    from the DB.
  cron.schedule('30 * * * *', () => {
    withCollectorRun('transportDiscovery', () => runTransportDiscovery(wsServer), { trigger: 'cron' })
      .catch((err) => {
        console.error('[scheduler] transport run failed:', err?.message);
      });
  });
}
