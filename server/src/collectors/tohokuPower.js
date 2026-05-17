/**
 * Tohoku でんき予報 power demand (tohoku-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'tohoku-power',
  metaSource: 'tohoku_power',
  operator: '東北電力ネットワーク',
  csvUrl: 'https://setsuden.nw.tohoku-epco.co.jp/common/demand/juyo_05_TOHOKU.csv',
  lat: 38.2682,
  lon: 140.8694,
});
