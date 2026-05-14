/**
 * Generic webhook channel. POSTs the alert payload as JSON to a
 * customer-supplied URL with an HMAC-SHA256 signature so receivers can
 * verify the call came from us.
 *
 * Headers on every request:
 *   Content-Type:      application/json
 *   X-JapanOSINT-Event: alert.fired
 *   X-JapanOSINT-Signature: t=<unix>,v1=<hex-hmac>
 *
 * Verification on the receiver side:
 *   signed_payload = `${t}.${raw_body}`
 *   expected       = HMAC_SHA256(secret, signed_payload)  // hex
 *   compare expected == v1 in constant time.
 *
 * The signing secret is a per-rule field (channel.secret), so tenants can
 * rotate independently. A 5-minute timestamp window blocks replays.
 *
 * Channel config per rule:
 *   { type: 'webhook', target: 'https://example.com/hook', secret: 'whsec_...' }
 */

import { createHmac, timingSafeEqual } from 'crypto';

export async function sendWebhook({ target, secret, event }) {
  if (!target || !/^https?:\/\//.test(target)) {
    throw new Error(`invalid webhook url: ${target?.slice(0, 40)}…`);
  }
  if (!secret || secret.length < 16) {
    throw new Error('webhook secret too short (min 16 chars)');
  }

  const body = JSON.stringify(event);
  const ts = Math.floor(Date.now() / 1000);
  const signedPayload = `${ts}.${body}`;
  const sig = createHmac('sha256', secret).update(signedPayload).digest('hex');

  const res = await fetch(target, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'X-JapanOSINT-Event': 'alert.fired',
      'X-JapanOSINT-Signature': `t=${ts},v1=${sig}`,
    },
    body,
  });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`webhook ${res.status}: ${text.slice(0, 200)}`);
  }
  return { ok: true, status: res.status };
}

/**
 * Receiver-side verification helper. Exported so test code and any future
 * inbound webhook endpoints can validate signatures using the same code
 * path. Returns true iff signature matches and timestamp is fresh.
 */
export function verifyWebhookSignature({ rawBody, header, secret, maxAgeSec = 300 }) {
  const m = /^t=(\d+),v1=([a-f0-9]+)$/i.exec(header || '');
  if (!m) return false;
  const ts = Number(m[1]);
  const sig = m[2];
  if (!Number.isFinite(ts)) return false;
  const now = Math.floor(Date.now() / 1000);
  if (Math.abs(now - ts) > maxAgeSec) return false;
  const expected = createHmac('sha256', secret).update(`${ts}.${rawBody}`).digest('hex');
  if (expected.length !== sig.length) return false;
  return timingSafeEqual(Buffer.from(expected, 'hex'), Buffer.from(sig, 'hex'));
}
