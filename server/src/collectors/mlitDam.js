/**
 * MLIT dam reservoir data (mlit-dam).
 *
 * The canonical MLIT dam feed (www1.river.go.jp/cgi-bin/DspDamData.exe)
 * serves only per-dam EUC-JP HTML behind an anti-automation policy — no
 * public JSON/GeoJSON listing or bulk endpoint exists. As a real,
 * policy-clean alternative we query OpenStreetMap via Overpass for dam
 * features in Japan (`waterway=dam` / `water=reservoir`). On any failure
 * the collector returns an honest empty FeatureCollection — never seeded.
 */

import { fetchOverpass } from './_liveHelpers.js';

const BODY = `
  way["waterway"="dam"](area.jp);
  relation["waterway"="dam"](area.jp);
  node["waterway"="dam"](area.jp);
`;

export default async function collectMlitDam() {
  let mapped = null;
  try {
    mapped = await fetchOverpass(
      BODY,
      (el, _i, coords) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          osm_id: `${el.type}/${el.id}`,
          name: el.tags?.name || el.tags?.['name:ja'] || el.tags?.['name:en'] || null,
          name_en: el.tags?.['name:en'] || null,
          operator: el.tags?.operator || null,
          height_m: el.tags?.height ? parseFloat(el.tags.height) : null,
          country: 'JP',
          source: 'osm_overpass_dam',
        },
      }),
      60000,
      { queryTimeout: 180 },
    );
  } catch {
    mapped = null;
  }

  const features = Array.isArray(mapped) ? mapped : [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'mlit_dam_osm' : 'mlit_dam_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Japanese dams (OSM Overpass; MLIT CGI has no public JSON feed)',
    },
  };
}
