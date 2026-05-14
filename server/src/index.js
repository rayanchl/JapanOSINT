import express from 'express';
import cors from 'cors';
import compression from 'compression';
import { createServer } from 'http';
import { WebSocketServer } from 'ws';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import { existsSync } from 'fs';

// Apply the api-keys overlay BEFORE importing any collector module — some
// collectors capture process.env[X] at module load and would otherwise miss
// values the user has set via the iOS API-keys tab.
import { applyOverlayToEnv } from './utils/apiKeysStore.js';
applyOverlayToEnv();

// Tenancy schema migration — additive only, idempotent. Runs every boot.
// Multi-tenant routing is gated by MULTI_TENANT_ENABLED so legacy
// single-tenant deployments keep working unchanged.
import { runTenancyMigration } from './utils/tenancyMigration.js';
runTenancyMigration();

import sourcesRouter from './routes/sources.js';
import layersRouter from './routes/layers.js';
import dataRouter from './routes/data.js';
import geocodeRouter from './routes/geocode.js';
import statusRouter from './routes/status.js';
import followRouter from './routes/follow.js';
import transitRouter from './routes/transit.js';
import dbRouter from './routes/db.js';
import plateauCatalogRouter from './routes/plateauCatalog.js';
import intelRouter from './routes/intel.js';
import apiKeysRouter from './routes/apiKeys.js';
import adminRouter from './routes/admin.js';
import breakGlassRouter from './routes/breakGlass.js';
import { requireSupabaseAuth, MULTI_TENANT_ENABLED } from './middleware/auth.js';
import { resolveTenant } from './middleware/tenant.js';
import { auditWriter } from './middleware/audit.js';
import { rateLimit } from './middleware/rateLimit.js';
import { startScheduler } from './utils/scheduler.js';
import { installFetchTap, setBroadcaster } from './utils/collectorTap.js';
import { runBulkHydrate } from './utils/gtfsBulkHydrate.js';
import { refreshFeedCatalogue, refreshRtFeedCatalogue } from './utils/gtfsStore.js';
import { refreshOdptTrainInformationAlerts } from './utils/odptToGtfsRt.js';
import { startRtPoller } from './utils/gtfsRtPoller.js';
import { startPlanePoller, stopPlanePoller } from './utils/planeAdsbPoller.js';
import { startShipPoller, stopShipPoller } from './utils/shipAisPoller.js';
import { rebuildAllAtBoot } from './utils/ftsRegistry.js';
import { ensureTokenizer } from './utils/jpTokenizer.js';
import cron from 'node-cron';

// Patch globalThis.fetch BEFORE importing any collector code that may
// capture a reference to it at module load time.
installFetchTap();

const __dirname = dirname(fileURLToPath(import.meta.url));
const PORT = process.env.PORT || 4000;

const app = express();
const server = createServer(app);

// ── Middleware ──────────────────────────────────────────────────────────
// Closed by default. ALLOWED_ORIGINS is a comma-separated list of origins
// that may call /api/* from a different host. Same-origin (the prod static
// serving below) and the Vite dev proxy don't need any entry here.
const allowedOrigins = (process.env.ALLOWED_ORIGINS || '')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean);
app.use(cors({
  origin: allowedOrigins.length === 0
    ? false
    : allowedOrigins.includes('*') ? '*' : allowedOrigins,
}));
app.use(compression());
app.use(express.json());

// ── Health check (BEFORE auth — must answer even during outages) ──────
app.get('/api/health', (_req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// ── Auth + tenant + audit + rate-limit (MULTI_TENANT_ENABLED only) ────
// Order matters: auth populates req.supabaseUser; tenant materialises
// req.tenant; audit + rate-limit both need req.tenant. Each middleware
// is itself flag-aware and no-ops when disabled, so this stack is safe
// to leave wired even in legacy single-tenant mode.
if (MULTI_TENANT_ENABLED) {
  app.use('/api', requireSupabaseAuth, resolveTenant, rateLimit, auditWriter);
  console.log('[boot] multi-tenant mode ON — Supabase auth + tenant resolution + rate-limit + audit active');
} else {
  console.log('[boot] multi-tenant mode OFF — running in legacy single-tenant mode');
}

// ── API Routes ─────────────────────────────────────────────────────────
app.use('/api/sources', sourcesRouter);
app.use('/api/layers', layersRouter);
app.use('/api/data', dataRouter);
app.use('/api/geocode', geocodeRouter);
app.use('/api/status', statusRouter);
app.use('/api/follow', followRouter);
app.use('/api/transit', transitRouter);
app.use('/api/db', dbRouter);
app.use('/api/plateau', plateauCatalogRouter);
app.use('/api/intel', intelRouter);
app.use('/api/keys', apiKeysRouter);
app.use('/api/admin', adminRouter);
// Break-glass admin path. Off unless BREAK_GLASS_ENABLED=1; the router
// itself 404s when disabled. Intentionally NOT under /api/admin so it
// can be reached without auth middleware getting in the way during an
// outage.
app.use('/admin/break-glass', breakGlassRouter);

// ── Serve static files in production ───────────────────────────────────
const clientDist = resolve(__dirname, '../../client/dist');
if (existsSync(clientDist)) {
  app.use(express.static(clientDist));
  app.get('*', (_req, res) => {
    res.sendFile(resolve(clientDist, 'index.html'));
  });
}

// ── WebSocket Server ───────────────────────────────────────────────────
const wss = new WebSocketServer({ server, path: '/ws' });

// Heartbeat: ping every 30s, terminate sockets that miss two beats. Without
// this, dead-but-not-yet-evicted clients accumulate in wss.clients forever
// behind NATs and proxies, and every broadcast pays for them.
const HEARTBEAT_MS = 30_000;

wss.on('connection', (ws) => {
  ws.isAlive = true;
  ws.send(JSON.stringify({ type: 'connected', timestamp: new Date().toISOString() }));

  ws.on('pong', () => { ws.isAlive = true; });

  ws.on('error', (err) => {
    console.error('[ws] Client error:', err.message);
  });

  ws.on('close', () => {
    ws.isAlive = false;
  });
});

const heartbeat = setInterval(() => {
  for (const ws of wss.clients) {
    if (ws.isAlive === false) {
      try { ws.terminate(); } catch { /* ignore */ }
      continue;
    }
    ws.isAlive = false;
    try { ws.ping(); } catch { /* ignore */ }
  }
}, HEARTBEAT_MS);

// ── Start scheduler & server ───────────────────────────────────────────
setBroadcaster(wss);

// Live-position pollers — push live_vehicle WS events only, no FTS writes,
// so they can start in parallel with kuromoji warmup. Idempotent and
// self-managed; ship poller is a no-op when no AIS API key is set.
startPlanePoller();
startShipPoller();

// Warm kuromoji, start the scheduler, then sequentially rebuild every
// registered FTS mirror. Routes mount immediately; readiness middleware
// (utils/ftsRegistry) returns 503 + Retry-After until each mirror's rebuild
// completes, then broadcasts an `fts_ready` WS event per table.
//
// The scheduler is gated on ensureTokenizer() because transport/camera
// collectors call upsertItemSync → segmentForFtsSync inside a sync txn.
// Starting it before kuromoji is warm would write raw (unsegmented) text
// for any row that lands in the first ~1 s of boot — the very race we're
// closing now that upsertItemSync writes FTS for real.
(async () => {
  try {
    await ensureTokenizer();
  } catch (err) {
    console.error('[index] kuromoji warmup failed — scheduler will not start; FTS would write unsegmented text:', err?.message);
    return;
  }
  startScheduler(wss);
  try {
    await rebuildAllAtBoot();
  } catch (err) {
    console.warn('[index] FTS boot rebuild failed:', err?.message);
  }
})();

// Nationwide GTFS hydrate — runs on the weekly cron below. Before the hydrate
// can run we must refresh the Shimada catalogue (gtfs_feeds) so
// listUpstreamOperatorIds has anything to return. Persists per-operator state
// in gtfs_operators.hydrated_at so a restart resumes from the next stale
// entry. The weekly cron re-refreshes the catalogue and re-hydrates anything
// past the 7-day freshness window.
async function refreshAndHydrate() {
  try {
    const r = await refreshOdptTrainInformationAlerts();
    console.log('[index] ODPT TrainInformation →', r);
  } catch (err) {
    console.warn('[index] ODPT TrainInformation refresh failed:', err?.message);
  }
  try {
    const { total } = await refreshFeedCatalogue();
    console.log(`[index] Shimada catalogue refreshed — ${total} feeds`);
  } catch (err) {
    console.error('[index] catalogue refresh failed:', err?.message);
    // Proceed anyway — stale catalogue is better than no hydrate at all.
  }
  try {
    await runBulkHydrate({ fresherThanDays: 7 });
  } catch (err) {
    console.error('[index] bulk hydrate failed:', err?.message);
  }
  // Seed gtfs_rt_feeds from ODPT-capable operators and (re)start the
  // GTFS-RT poller. Safe to call repeatedly — startRtPoller cancels any
  // existing timers first. No-op when ODPT_CHALLENGE_TOKEN is unset.
  try {
    const rtSeed = refreshRtFeedCatalogue();
    console.log('[index] RT feed catalogue seed →', rtSeed);
    startRtPoller();
  } catch (err) {
    console.warn('[index] RT poller start failed:', err?.message);
  }
}

const weeklyHydrateTask = cron.schedule(
  '0 3 * * 0',
  () => { refreshAndHydrate(); },
  { timezone: 'Asia/Tokyo' },
);

// Every 5 minutes: refresh ODPT TrainInformation. Status can change fast
// during rush hour disruptions.
const trainInfoTask = cron.schedule(
  '*/5 * * * *',
  () => {
    refreshOdptTrainInformationAlerts().catch((err) => {
      console.warn('[index] scheduled TrainInformation refresh failed:', err?.message);
    });
  },
  { timezone: 'Asia/Tokyo' },
);

// ── 500 handler ────────────────────────────────────────────────────────
// Last-resort catch for anything a route forwarded via next(err). Keeps the
// response shape predictable and never leaks stack traces. Routes that hit
// expected failure modes still return their own JSON; this only fires for
// truly unexpected errors.
// eslint-disable-next-line no-unused-vars
app.use((err, req, res, _next) => {
  console.error(`[express] ${req.method} ${req.originalUrl}:`, err?.stack || err?.message || err);
  if (res.headersSent) return;
  res.status(500).json({ error: 'internal_error' });
});

// ── Process-level safety nets ──────────────────────────────────────────
process.on('uncaughtException', (err) => {
  console.error('[process] uncaughtException:', err?.stack || err);
});
process.on('unhandledRejection', (reason) => {
  console.error('[process] unhandledRejection:', reason?.stack || reason);
});

// ── Graceful shutdown ──────────────────────────────────────────────────
let shuttingDown = false;
async function shutdown(signal) {
  if (shuttingDown) return;
  shuttingDown = true;
  console.log(`[server] received ${signal}, shutting down`);
  clearInterval(heartbeat);
  try { weeklyHydrateTask.stop(); } catch { /* ignore */ }
  try { trainInfoTask.stop(); } catch { /* ignore */ }
  try { stopPlanePoller(); } catch { /* ignore */ }
  try { stopShipPoller(); } catch { /* ignore */ }
  // node-cron exposes the active task list; stop everything we registered.
  try {
    for (const task of cron.getTasks().values()) {
      try { task.stop(); } catch { /* ignore */ }
    }
  } catch { /* ignore */ }
  // Tell every connected client we're going away, then close the WS server.
  for (const ws of wss.clients) {
    try { ws.close(1001, 'server_shutdown'); } catch { /* ignore */ }
  }
  wss.close();
  // Give in-flight HTTP requests up to 10s before forcing exit.
  const force = setTimeout(() => {
    console.warn('[server] forced exit after 10s grace period');
    process.exit(1);
  }, 10_000);
  force.unref();
  server.close(() => {
    clearTimeout(force);
    process.exit(0);
  });
}
process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));

server.listen(PORT, () => {
  console.log(`[server] Japan OSINT Map backend running on http://localhost:${PORT}`);
  console.log(`[server] WebSocket available at ws://localhost:${PORT}/ws`);
});

export default app;
