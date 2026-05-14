/**
 * Bluesky Jetstream — short JP-language window snapshot.
 *
 * Bluesky Jetstream is a websocket. To avoid keeping a long-lived socket
 * open inside a request handler we open it briefly (default 8s), collect
 * any `app.bsky.feed.post` records whose langs contain "ja", then close.
 *
 * Free, no auth.
 */

import { WebSocket } from 'ws';

const URL = 'wss://jetstream2.us-west.bsky.network/subscribe?wantedCollections=app.bsky.feed.post';
const TIMEOUT_MS = Number(process.env.BLUESKY_JETSTREAM_WINDOW_MS || 8000);
const MAX_POSTS = Number(process.env.BLUESKY_JETSTREAM_MAX || 200);
const TOKYO = [139.6917, 35.6895];

export default async function collectBlueskyJetstreamJp() {
  return new Promise((resolve) => {
    const features = [];
    let ws;
    let closeTimer = null;
    let done = false;
    const finish = (live, err) => {
      if (done) return; done = true;
      if (closeTimer) { clearTimeout(closeTimer); closeTimer = null; }
      // Detach handlers before close so a late 'message' or 'error' from the
      // socket teardown can't re-enter finish or push into features after the
      // promise has resolved.
      if (ws) {
        try { ws.removeAllListeners(); } catch { /* ignore */ }
        try { ws.close(); } catch { /* ignore */ }
      }
      resolve({
        type: 'FeatureCollection',
        features,
        _meta: {
          source: live ? 'bluesky_jetstream' : 'bluesky_jetstream_seed',
          fetchedAt: new Date().toISOString(),
          recordCount: features.length,
          window_ms: TIMEOUT_MS,
          err: err || null,
          description: 'Bluesky Jetstream — JP-lang post snapshot',
        },
      });
    };

    try {
      ws = new WebSocket(URL);
    } catch (err) {
      finish(false, err?.message);
      return;
    }

    closeTimer = setTimeout(() => finish(true, null), TIMEOUT_MS);

    ws.on('error', (err) => finish(false, err?.message || 'ws_error'));

    ws.on('message', (raw) => {
      if (done) return;
      if (features.length >= MAX_POSTS) { finish(true, null); return; }
      let msg;
      try { msg = JSON.parse(raw.toString()); } catch { return; }
      const op = msg?.commit;
      if (!op || op.collection !== 'app.bsky.feed.post' || op.operation !== 'create') return;
      const langs = op?.record?.langs || [];
      if (!langs.includes('ja')) return;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          did: msg.did,
          rkey: op.rkey,
          time_us: msg.time_us,
          text: String(op?.record?.text || '').slice(0, 400),
          langs,
          embed_type: op?.record?.embed?.$type || null,
          source: 'bluesky_jetstream',
        },
      });
    });
  });
}
