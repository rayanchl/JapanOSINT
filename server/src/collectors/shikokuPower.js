/**
 * Shikoku (Yonden) でんき予報 power demand (shikoku-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'shikoku-power',
  metaSource: 'shikoku_power',
  operator: '四国電力送配電',
  csvUrl: 'https://www.yonden.co.jp/nw/denkiyoho/csv/juyo_shikoku.csv',
  lat: 34.3401,
  lon: 134.0434,
});
