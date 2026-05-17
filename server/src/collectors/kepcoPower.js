/**
 * KEPCO (Kansai) でんき予報 power demand (kepco-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'kepco-power',
  metaSource: 'kepco_power',
  operator: '関西電力送配電',
  csvUrl: 'https://www.kansai-td.co.jp/yamasou/juyo1_kansai.csv',
  lat: 34.6937,
  lon: 135.5023,
});
