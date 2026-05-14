/**
 * MLIT N05 — National Land Numerical Information: Rail Network (historical)
 * 国土数値情報 鉄道（時系列）N05
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N05-v2_0.html
 *
 * Surfaced in the app as "Abandoned Rail" — we filter to abolished segments
 * only. Active rail is already covered by unified-trains.
 */

import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

export default createMlitKsjCollector({
  code: 'N05',
  envKey: 'MLIT_N05_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-22/N05-22.geojson',
    'https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-21/N05-21.geojson',
  ],
  filter: (f) => f.properties?.status === 'abolished',
  description: 'Abandoned JP rail segments from MLIT KSJ N05 (filtered to status=abolished only)',
  envHint: 'Set MLIT_N05_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N05 (long-term rail history).',
});
