/**
 * CertStream background subscriber — maintains a ring buffer of recent
 * `.jp` domain certificate issuance events from the public CertStream
 * WebSocket relay (calidog.io).
 *
 * Runs in-process as soon as the module loads; reconnects with exponential
 * backoff; caps the buffer at MAX_EVENTS to bound memory.
 *
 * Event shape (as reported by CertStream):
 *   { message_type: 'certificate_update',
 *     data: { cert_link, seen, source: {name, url},
 *             leaf_cert: { subject:{CN}, all_domains, not_before, not_after } } }
 *
 * We only keep events where ANY of all_domains ends in `.jp`.
 *
 * No auth. No rate limit. MIT-licensed public relay.
 */

import WebSocket from 'ws';

const CERTSTREAM_URL = 'wss://certstream.calidog.io/';
const MAX_EVENTS = 2000;
const RECONNECT_BASE_MS = 3000;

let ws = null;
let retries = 0;
const buffer = []; // newest first

function push(ev) {
  buffer.unshift(ev);
  if (buffer.length > MAX_EVENTS) buffer.length = MAX_EVENTS;
}

function connect() {
  try {
    ws = new WebSocket(CERTSTREAM_URL, {
      headers: { 'user-agent': 'JapanOSINT/1.0' },
      perMessageDeflate: false,
    });

    ws.on('open', () => {
      retries = 0;
      console.log('[certstream] connected');
    });

    ws.on('message', (raw) => {
      let msg;
      try { msg = JSON.parse(raw.toString()); } catch { return; }
      if (msg.message_type !== 'certificate_update') return;
      const leaf = msg.data?.leaf_cert;
      if (!leaf) return;
      const allDomains = Array.isArray(leaf.all_domains) ? leaf.all_domains : [];
      const jpDomains = allDomains.filter((d) => typeof d === 'string' && d.toLowerCase().endsWith('.jp'));
      if (jpDomains.length === 0) return;
      push({
        ts: Date.now(),
        seen: msg.data.seen,
        cn: leaf.subject?.CN || null,
        issuer: leaf.issuer?.O || null,
        jp_domains: jpDomains,
        all_domains: allDomains,
        not_before: leaf.not_before,
        not_after: leaf.not_after,
        ct_source: msg.data.source?.name || null,
        cert_link: msg.data.cert_link || null,
      });
    });

    ws.on('close', () => {
      console.log('[certstream] disconnected, retrying');
      scheduleReconnect();
    });

    ws.on('error', (err) => {
      console.warn('[certstream] error:', err?.message);
      try { ws.close(); } catch {}
    });
  } catch (err) {
    console.warn('[certstream] connect threw:', err?.message);
    scheduleReconnect();
  }
}

function scheduleReconnect() {
  ws = null;
  retries += 1;
  const delay = Math.min(RECONNECT_BASE_MS * 2 ** Math.min(retries, 5), 5 * 60 * 1000);
  setTimeout(connect, delay);
}

let started = false;
export function startCertstream() {
  if (started) return;
  started = true;
  connect();
}

export function getRecentJpCerts({ limit = 500 } = {}) {
  return buffer.slice(0, Math.max(1, Math.min(MAX_EVENTS, limit)));
}

export function getBufferSize() {
  return buffer.length;
}
