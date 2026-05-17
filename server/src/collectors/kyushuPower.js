/**
 * Kyushu (Kyuden) でんき予報 power demand (kyushu-power).
 * Real public hourly supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'kyushu-power',
  metaSource: 'kyushu_power',
  operator: '九州電力送配電',
  csvUrl: 'https://www.kyuden.co.jp/td_power_usages/csv/juyo-hourly-Kyushu.csv',
  lat: 33.5902,
  lon: 130.4017,
});
