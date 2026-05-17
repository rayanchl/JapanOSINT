/**
 * JMA / MOE pollen forecast collector (jma-pollen) — kind:'intel'.
 *
 * Japan's authoritative pollen-observation system is the Ministry of the
 * Environment "はなこさん" (kafun.env.go.jp); the structured per-prefecture
 * concentration data behind it is already mirrored by the kafun-pollen
 * collector. JMA itself does not publish a separate key-free machine-readable
 * pollen JSON feed. To avoid fabricating values we emit a single portal intel
 * item recording the official はなこさん endpoint plus a reachability probe.
 *
 * NOTE: a dedicated JMA pollen JSON endpoint is UNVERIFIED / believed absent.
 */

import { fetchHead } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jma-pollen';
const PORTAL = 'https://kafun.env.go.jp/';

export default async function collectJmaPollen() {
  const live = await fetchHead(PORTAL, { timeoutMs: 8000 }).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Pollen forecast (花粉飛散予測 / はなこさん)',
      summary: 'Per-prefecture pollen concentration — cedar / cypress / grasses (MOE はなこさん)',
      body: 'Authoritative pollen observation is the MOE はなこさん system; no separate key-free JMA pollen JSON endpoint exists, so no point features are emitted here.',
      link: PORTAL,
      author: '環境省 Ministry of the Environment',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['pollen', 'environment', 'health', live ? 'reachable' : 'unreachable'],
      properties: { operator: '環境省', reachable: !!live, machine_readable: false },
    }],
    live: !!live,
    description: 'Pollen forecast portal (はなこさん) — portal item only',
  });
}
