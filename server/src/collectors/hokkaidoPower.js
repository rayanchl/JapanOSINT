/**
 * Hokkaido (HEPCO) でんき予報 power demand (hokkaido-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'hokkaido-power',
  metaSource: 'hokkaido_power',
  operator: '北海道電力ネットワーク',
  csvUrl: 'https://denkiyoho.hepco.co.jp/area/data/juyo_01_hokkaido.csv',
  lat: 43.0642,
  lon: 141.3469,
});
