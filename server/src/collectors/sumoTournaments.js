/**
 * JSA — official sumo tournament (本場所) calendar + bout data.
 * https://www.sumo.or.jp/EnHonbashoMain/torikumi/
 *
 * 6 honbasho per year (3 in Tokyo + Osaka, Nagoya, Fukuoka). Per-day
 * bout matchups (取組), result, injury reports, kosho ban announcements.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'sumo-tournaments';
const PROBE_URL = 'https://www.sumo.or.jp/EnHonbashoMain/torikumi/';

export default async function collectSumoTournaments() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JSA — sumo tournament (本場所) data',
      summary: '6 honbasho per year, per-day bout matchups + results + injury reports',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['sumo', 'sport', 'jsa', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Japan Sumo Association', reachable: live },
    }],
    live,
    description: 'JSA sumo tournament calendar + bout data',
  });
}
