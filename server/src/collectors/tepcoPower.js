/**
 * TEPCO でんき予報 power demand (tepco-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'tepco-power',
  metaSource: 'tepco_power',
  operator: '東京電力パワーグリッド',
  csvUrl: 'https://www.tepco.co.jp/forecast/html/images/juyo-d-j.csv',
  lat: 35.6762,
  lon: 139.6503,
});
