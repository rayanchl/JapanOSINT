import cron from 'node-cron';
import sources from './sourceRegistry.js';
import {
  upsertSource,
  updateSourceStatus,
  logFetch,
} from './database.js';

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
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15_000);

    const res = await fetch(probeUrlFor(source.url), { signal: controller.signal });
    clearTimeout(timeout);

    const duration = Date.now() - start;
    const ok = res.ok;

    // Try to count records if the response is JSON
    let recordsCount = 0;
    try {
      const body = await res.text();
      const json = JSON.parse(body);
      if (Array.isArray(json)) {
        recordsCount = json.length;
      } else if (json && typeof json === 'object') {
        // Try common wrapper patterns
        const data = json.data ?? json.results ?? json.features ?? json.items;
        if (Array.isArray(data)) recordsCount = data.length;
        else recordsCount = 1;
      }
    } catch {
      // Not JSON or unreadable – that's fine
    }

    const status = ok ? 'online' : 'degraded';

    updateSourceStatus({
      id: source.id,
      status,
      response_time_ms: duration,
      records_count: recordsCount || null,
      error_message: ok ? null : `HTTP ${res.status}`,
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
 * Convert an updateInterval (in seconds) to a cron expression.
 * Only approximate – cron doesn't support arbitrary second intervals,
 * so we map to the nearest sensible schedule.
 */
function intervalToCron(seconds) {
  if (seconds <= 60) return '* * * * *';            // every minute
  if (seconds <= 300) return '*/5 * * * *';          // every 5 min
  if (seconds <= 600) return '*/10 * * * *';         // every 10 min
  if (seconds <= 1800) return '*/30 * * * *';        // every 30 min
  if (seconds <= 3600) return '0 * * * *';           // every hour
  if (seconds <= 21600) return '0 */6 * * *';        // every 6 hours
  if (seconds <= 86400) return '0 0 * * *';          // daily
  return '0 0 * * 0';                                // weekly
}

/**
 * Start the scheduler: seed the database, schedule periodic fetches.
 * @param {import('ws').WebSocketServer} wsServer
 */
export function startScheduler(wsServer) {
  // 1. Register all sources in the database
  for (const src of sources) {
    upsertSource({
      id: src.id,
      name: src.name,
      type: src.type,
      category: src.category,
      url: src.url,
      status: src.status ?? 'offline',
    });
  }

  console.log(`[scheduler] Registered ${sources.length} sources in database`);

  // 2. Schedule periodic fetches for free API sources
  const schedulable = sources.filter(
    (s) => s.free && s.type === 'api' && s.url,
  );

  for (const src of schedulable) {
    const cronExpr = intervalToCron(src.updateInterval);
    cron.schedule(cronExpr, () => {
      fetchSource(src, wsServer).catch((err) => {
        console.error(`[scheduler] Unhandled error fetching ${src.id}:`, err);
      });
    });
  }

  console.log(`[scheduler] Scheduled ${schedulable.length} free API source jobs`);

  // 3. Run an initial health-check sweep after a short delay
  setTimeout(() => {
    console.log('[scheduler] Running initial health-check sweep...');
    for (const src of schedulable) {
      // Stagger requests to avoid thundering herd
      const delay = Math.random() * 30_000;
      setTimeout(() => {
        fetchSource(src, wsServer).catch(() => {});
      }, delay);
    }
  }, 5_000);

  // 4. Broadcast a heartbeat every 30 seconds
  setInterval(() => {
    broadcast(wsServer, { type: 'heartbeat', timestamp: new Date().toISOString() });
  }, 30_000);
}
