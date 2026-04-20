import express from 'express';
import cors from 'cors';
import compression from 'compression';
import { createServer } from 'http';
import { WebSocketServer } from 'ws';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import { existsSync } from 'fs';

import sourcesRouter from './routes/sources.js';
import layersRouter from './routes/layers.js';
import dataRouter from './routes/data.js';
import geocodeRouter from './routes/geocode.js';
import statusRouter from './routes/status.js';
import followRouter from './routes/follow.js';
import transitRouter from './routes/transit.js';
import dbRouter from './routes/db.js';
import { startScheduler } from './utils/scheduler.js';
import { installFetchTap, setBroadcaster } from './utils/collectorTap.js';
import { runBulkHydrate } from './utils/gtfsBulkHydrate.js';
import { refreshFeedCatalogue } from './utils/gtfsStore.js';
import { refreshOdptTrainInformationAlerts } from './utils/odptToGtfsRt.js';
import cron from 'node-cron';

// Patch globalThis.fetch BEFORE importing any collector code that may
// capture a reference to it at module load time.
installFetchTap();

const __dirname = dirname(fileURLToPath(import.meta.url));
const PORT = process.env.PORT || 4000;

const app = express();
const server = createServer(app);

// ── Middleware ──────────────────────────────────────────────────────────
app.use(cors());
app.use(compression());
app.use(express.json());

// ── API Routes ─────────────────────────────────────────────────────────
app.use('/api/sources', sourcesRouter);
app.use('/api/layers', layersRouter);
app.use('/api/data', dataRouter);
app.use('/api/geocode', geocodeRouter);
app.use('/api/status', statusRouter);
app.use('/api/follow', followRouter);
app.use('/api/transit', transitRouter);
app.use('/api/db', dbRouter);

// ── Health check ───────────────────────────────────────────────────────
app.get('/api/health', (_req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

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

wss.on('connection', (ws) => {
  ws.send(JSON.stringify({ type: 'connected', timestamp: new Date().toISOString() }));

  ws.on('error', (err) => {
    console.error('[ws] Client error:', err.message);
  });
});

// ── Start scheduler & server ───────────────────────────────────────────
setBroadcaster(wss);
startScheduler(wss);

// Nationwide GTFS hydrate — kicks off 15s after boot so the HTTP server and
// scheduler get a chance to warm up first. Before the hydrate can run we
// must refresh the Shimada catalogue (gtfs_feeds) so listUpstreamOperatorIds
// has anything to return. Persists per-operator state in
// gtfs_operators.hydrated_at so a restart resumes from the next stale entry.
// The weekly cron re-refreshes the catalogue and re-hydrates anything past
// the 7-day freshness window.
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
}

setTimeout(() => { refreshAndHydrate(); }, 15_000);
cron.schedule(
  '0 3 * * 0',
  () => { refreshAndHydrate(); },
  { timezone: 'Asia/Tokyo' },
);

// Every 5 minutes: refresh ODPT TrainInformation. Status can change fast
// during rush hour disruptions.
cron.schedule(
  '*/5 * * * *',
  () => {
    refreshOdptTrainInformationAlerts().catch((err) => {
      console.warn('[index] scheduled TrainInformation refresh failed:', err?.message);
    });
  },
  { timezone: 'Asia/Tokyo' },
);

server.listen(PORT, () => {
  console.log(`[server] Japan OSINT Map backend running on http://localhost:${PORT}`);
  console.log(`[server] WebSocket available at ws://localhost:${PORT}/ws`);
});

export default app;
