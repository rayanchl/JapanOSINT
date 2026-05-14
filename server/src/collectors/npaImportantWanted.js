/**
 * NPA Important Wanted Persons (警察庁指定重要指名手配被疑者).
 *
 * Source: `jyuyo1.html` / `jyuyo2.html` under www.npa.go.jp/bureau/criminal/wanted/
 * — the small list of high-profile suspects the NPA itself designates
 * (separate from the much larger pool of prefectural wanted-persons cases
 * scraped by `wantedPersons.js`).
 *
 * Each section on the page describes one suspect: name, kana, age, photo
 * URL, height, alleged crime, and a link to the prefectural police force
 * handling the case. The collector parses these into spatial features
 * pinned at the handling prefecture's centroid (with small jitter when
 * multiple suspects share a prefecture).
 *
 * Public domain via NPA. The layer is flagged `sensitive` in the frontend
 * so it requires explicit user opt-in before rendering.
 */

import { fetchText } from './_liveHelpers.js';
import { JP_PREFECTURES, resolvePrefecture } from './_jpPrefectures.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'npa-important-wanted';
const PAGES = [
  'https://www.npa.go.jp/bureau/criminal/wanted/jyuyo1.html',
  'https://www.npa.go.jp/bureau/criminal/wanted/jyuyo2.html',
];

const NPA_BASE = 'https://www.npa.go.jp';

// Map from issuing-force keywords → prefecture lookup. Some links use
// the police force's own domain (keishicho.metro.tokyo.lg.jp → Tokyo);
// others embed the prefecture name in the link text. We try both.
const HOST_TO_PREFECTURE = {
  'keishicho.metro.tokyo.lg.jp': '13',
  'police.pref.kanagawa.jp': '14',
  'police.pref.osaka.lg.jp': '27',
  'police.pref.hyogo.lg.jp': '28',
  'police.pref.fukuoka.jp': '40',
  'police.pref.chiba.jp': '12',
  'police.pref.saitama.lg.jp': '11',
  'police.pref.aichi.jp': '23',
  'police.pref.hokkaido.lg.jp': '01',
  'police.pref.miyagi.jp': '04',
  'police.pref.gunma.jp': '10',
  'police.pref.aomori.jp': '02',
  'police.pref.akita.lg.jp': '05',
  'police.pref.fukushima.jp': '07',
  'police.pref.mie.jp': '24',
  'police.pref.hiroshima.lg.jp': '34',
  'police.pref.ehime.jp': '38',
  'police.pref.kochi.lg.jp': '39',
  'police.pref.nagasaki.jp': '42',
  'police.pref.kumamoto.jp': '43',
  'police.pref.nara.jp': '29',
  'police.pref.saga.jp': '41',
  'police.pref.oita.jp': '44',
  'police.pref.okinawa.jp': '47',
  'police.pref.yamaguchi.jp': '35',
  'police.pref.wakayama.lg.jp': '30',
};

const SECTION_RE = /<section>([\s\S]*?)<\/section>/g;
const NAME_RE   = /<h2>(?:<a[^>]*>)?([^<（]+?)(?:\s*（(\d+)歳）)?(?:<\/a>)?<\/h2>/;
const IMG_RE    = /<img[^>]*src="([^"]+)"/;
const BODY_RE   = /<p>([\s\S]*?)<\/p>/;
const HEIGHT_RE = /身長([0-9０-９ｃcｍm]+)/;
const HANDLER_LINK_RE = /<li>\s*<a[^>]*href="([^"]+)"[^>]*>([^<]+)<\/a>/;

function resolveHandlingPrefecture(url, label) {
  try {
    const u = new URL(url, NPA_BASE);
    const code = HOST_TO_PREFECTURE[u.hostname];
    if (code) return JP_PREFECTURES.find((p) => p.code === code) || null;
  } catch { /* fall through */ }
  // Try to extract a prefecture name from the link label
  // e.g. "群馬県警察手配(群馬県警察ホームページ)" → 群馬県
  const m = (label || '').match(/(?:[一-鿿]{2,4}(?:都|道|府|県))/);
  if (m) {
    const r = resolvePrefecture(m[0]);
    if (r) return r;
  }
  // Tokyo MPD's name is 警視庁 — explicit fallback
  if (/警視庁/.test(label || '')) return JP_PREFECTURES.find((p) => p.code === '13') || null;
  return null;
}

function parsePage(html) {
  const out = [];
  SECTION_RE.lastIndex = 0;
  let m;
  while ((m = SECTION_RE.exec(html))) {
    const block = m[1];
    if (!/jyuyo|wanted|手配/.test(block) && !/写真/.test(block)) continue;
    const nameMatch = block.match(NAME_RE);
    const imgMatch = block.match(IMG_RE);
    const bodyMatch = block.match(BODY_RE);
    const handlerMatch = block.match(HANDLER_LINK_RE);
    if (!nameMatch || !imgMatch) continue;

    const name = nameMatch[1].replace(/\s+/g, ' ').trim();
    const ageStr = nameMatch[2];
    const age = ageStr ? parseInt(ageStr, 10) : null;
    const photoUrl = imgMatch[1].startsWith('http')
      ? imgMatch[1]
      : `${NPA_BASE}${imgMatch[1].startsWith('/') ? '' : '/bureau/criminal/wanted/'}${imgMatch[1]}`;
    const bodyTxt = (bodyMatch?.[1] || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    const heightMatch = bodyTxt.match(HEIGHT_RE);
    const height = heightMatch ? heightMatch[1] : null;
    const crime = bodyTxt
      .replace(/身長[0-9０-９ｃcｍm]+\s*/g, '')
      .replace(/位\s*/, '')
      .trim();

    const handlerUrl = handlerMatch?.[1] || null;
    const handlerLabel = handlerMatch?.[2] || null;
    const pref = handlerUrl ? resolveHandlingPrefecture(handlerUrl, handlerLabel) : null;

    out.push({ name, age, height, crime, photoUrl, handlerUrl, handlerLabel, prefecture: pref });
  }
  return out;
}

function jitter(lat, lon, i, n) {
  if (n <= 1) return [lon, lat];
  const angle = (i / n) * 2 * Math.PI;
  const r = 0.04 + (i % 3) * 0.015;
  return [lon + Math.cos(angle) * r, lat + Math.sin(angle) * r];
}

export default async function collectNpaImportantWanted() {
  const fetchedAt = new Date().toISOString();
  const all = [];
  for (const url of PAGES) {
    const html = await fetchText(url, { timeoutMs: 10000, retries: 1 });
    if (!html) continue;
    const entries = parsePage(html);
    for (const e of entries) all.push({ ...e, sourcePage: url });
  }

  // Group by prefecture for jitter
  const byPref = new Map();
  for (const e of all) {
    const code = e.prefecture?.code || 'XX';
    if (!byPref.has(code)) byPref.set(code, []);
    byPref.get(code).push(e);
  }

  const features = [];
  const intelItems = [];
  for (const [code, entries] of byPref) {
    const list = entries;
    list.forEach((e, i) => {
      const pref = e.prefecture;
      if (pref) {
        const [lon, lat] = jitter(pref.lat, pref.lon, i, list.length);
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            id: `JYUYO_${code}_${i + 1}`,
            name: e.name,
            age: e.age,
            height: e.height,
            crime: e.crime,
            photo_url: e.photoUrl,
            handler_url: e.handlerUrl,
            handler_label: e.handlerLabel,
            prefecture_code: pref.code,
            prefecture_ja: pref.ja,
            sensitive: true,
            source: SOURCE_ID,
          },
        });
      } else {
        // No resolvable handling prefecture — surface as intel item only.
        intelItems.push({
          uid: intelUid(SOURCE_ID, e.name),
          title: `指名手配: ${e.name}${e.age ? ` (${e.age})` : ''}`,
          summary: e.crime,
          link: e.handlerUrl || e.sourcePage,
          language: 'ja',
          published_at: fetchedAt,
          tags: ['crime', 'wanted', 'sensitive', 'unresolved-prefecture'],
          properties: {
            name: e.name,
            age: e.age,
            height: e.height,
            crime: e.crime,
            photo_url: e.photoUrl,
            handler_url: e.handlerUrl,
            handler_label: e.handlerLabel,
          },
        });
      }
    });
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'npa_jyuyo_html' : null,
      upstream_url: PAGES[0],
      sensitive: true,
      description: 'NPA-designated 重要指名手配 high-profile wanted persons (photos public-domain via NPA). Sensitive layer — frontend requires opt-in.',
    },
    intel: { items: intelItems },
  };
}
