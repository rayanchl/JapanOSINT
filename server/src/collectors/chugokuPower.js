/**
 * Chugoku (Energia) でんき予報 power demand (chugoku-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'chugoku-power',
  metaSource: 'chugoku_power',
  operator: '中国電力ネットワーク',
  csvUrl: 'https://www.energia.co.jp/nw/jukyuu/sys/juyo-d1-j.csv',
  lat: 34.3966,
  lon: 132.4596,
});
