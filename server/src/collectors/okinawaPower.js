/**
 * Okinawa (Okiden) でんき予報 power demand (okinawa-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'okinawa-power',
  metaSource: 'okinawa_power',
  operator: '沖縄電力',
  csvUrl: 'https://www.okiden.co.jp/denki2/juyo_10_okinawa.csv',
  lat: 26.2124,
  lon: 127.6792,
});
