/**
 * Chubu (CHUDEN) でんき予報 power demand (chubu-power).
 * Real public 5-minute supply/demand CSV. Honest empty on failure.
 */
import { makeDenkiYohoCollector } from './_denkiYoho.js';

export default makeDenkiYohoCollector({
  sourceId: 'chubu-power',
  metaSource: 'chubu_power',
  operator: '中部電力パワーグリッド',
  csvUrl: 'https://denki-yoho.chuden.jp/denki_yoho_content_data/juyo_cepco003.csv',
  lat: 35.1815,
  lon: 136.9066,
});
