/**
 * Kakaku.com price-intelligence portal.
 * https://kakaku.com/
 *
 * The canonical JP electronics price tracker — also covers cars,
 * appliances, broadband ISP plans, real estate, insurance. Crosscheck
 * with `mercari-trending` for demand-side, `suumo-rental-density` for
 * real-estate price-of-listing trend.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'kakaku-prices';
const PROBE_URL = 'https://kakaku.com/';

export default async function collectKakakuPrices() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Kakaku.com price intelligence',
      summary: 'JP price tracker — electronics, cars, appliances, ISP plans, real estate',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['marketplace', 'price', 'kakaku', live ? 'reachable' : 'unreachable'],
      properties: { operator: '価格.com', reachable: live },
    }],
    live,
    description: 'Kakaku.com price-intelligence portal',
  });
}
