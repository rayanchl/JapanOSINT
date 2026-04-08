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
import { startScheduler } from './utils/scheduler.js';

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
startScheduler(wss);

server.listen(PORT, () => {
  console.log(`[server] Japan OSINT Map backend running on http://localhost:${PORT}`);
  console.log(`[server] WebSocket available at ws://localhost:${PORT}/ws`);
});

export default app;
