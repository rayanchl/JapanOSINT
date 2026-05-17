/**
 * JMA Himawari satellite imagery collector (jma-himawari) — kind:'intel'.
 *
 * Himawari-9 imagery is raster, not point data, so this emits intel items
 * linking to the latest available imagery rather than fabricating features.
 *
 * The key-free public index of the latest available frames is:
 *   https://www.jma.go.jp/bosai/himawari/data/satimg/targetTimes_fd.json
 * (array of { basetime, validtime } in JST "yyyymmddHHMMSS"). The tiled PNG
 * imagery itself lives under
 *   https://www.jma.go.jp/bosai/himawari/data/satimg/<basetime>/fd/<validtime>/<band>/<z>/<x>/<y>.jpg
 * We surface the latest full-disk frame timestamp plus the interactive viewer
 * link. Honest empty on fetch failure — never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jma-himawari';
const TARGET_TIMES_FD = 'https://www.jma.go.jp/bosai/himawari/data/satimg/targetTimes_fd.json';
const VIEWER = 'https://www.jma.go.jp/bosai/map.html#5/34.5/137/&elem=satellite&contents=himawari';

// "20260516060000" (JST) → ISO 8601 string with +09:00 offset.
function jstStampToIso(s) {
  if (typeof s !== 'string' || s.length !== 14) return null;
  const y = s.slice(0, 4), mo = s.slice(4, 6), d = s.slice(6, 8);
  const h = s.slice(8, 10), mi = s.slice(10, 12), se = s.slice(12, 14);
  const dt = new Date(`${y}-${mo}-${d}T${h}:${mi}:${se}+09:00`);
  return Number.isNaN(dt.getTime()) ? null : dt.toISOString();
}

export default async function collectJmaHimawari() {
  const times = await fetchJson(TARGET_TIMES_FD, { timeoutMs: 8000 }).catch(() => null);
  if (!Array.isArray(times) || times.length === 0) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'JMA Himawari-9 satellite imagery — upstream unavailable',
    });
  }

  const latest = times[times.length - 1];
  const basetime = latest?.basetime ?? null;
  const validtime = latest?.validtime ?? null;
  const observedIso = jstStampToIso(validtime);
  // Visible band (B13 = IR is also common); B03 is true-colour-ish daytime.
  const tileTemplate = basetime && validtime
    ? `https://www.jma.go.jp/bosai/himawari/data/satimg/${basetime}/fd/${validtime}/B13/TBB/{z}/{x}/{y}.jpg`
    : null;

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, validtime || 'latest'),
      title: `Himawari-9 full-disk imagery${observedIso ? ` — ${observedIso}` : ''}`,
      summary: 'Latest available JMA Himawari-9 geostationary satellite full-disk frame',
      body: `Latest Himawari-9 full-disk frame basetime=${basetime} validtime=${validtime}. Imagery is raster tiles served by JMA bosai; open the interactive viewer for the live mosaic.`,
      link: VIEWER,
      author: '気象庁 Japan Meteorological Agency',
      language: 'ja',
      published_at: observedIso || new Date().toISOString(),
      tags: ['satellite', 'himawari', 'jma', 'imagery'],
      properties: {
        basetime,
        validtime,
        observed_at: observedIso,
        tile_template: tileTemplate,
        frames_available: times.length,
      },
    }],
    live: true,
    description: 'JMA Himawari-9 latest full-disk satellite imagery frame',
  });
}
