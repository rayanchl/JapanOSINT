/**
 * Hokuriku (Rikuden) でんき予報 power demand (hokuriku-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'hokuriku-power',
  metaSource: 'hokuriku_power',
  operator: '北陸電力送配電',
  csvUrl: 'https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_rikuden.csv',
  lat: 36.6953,
  lon: 137.2113,
});
