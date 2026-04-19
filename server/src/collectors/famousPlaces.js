/**
 * Famous Places / OSM POIs — unified collector (infrastructure category)
 *
 * Nationwide sweep of every OSM feature that qualifies as a "famous place" or
 * noteworthy POI in Japan. Covers the full scope of the former Famous Places
 * collector AND the OSM-tourism fallback that used to live inside the Google
 * My Maps collector.
 *
 * Source: OSM Overpass API, tiled across 12 bboxes that cover the whole of
 * Japan (main islands + Hokkaido + Okinawa + Ogasawara). Nodes AND ways; ways
 * return their `center` as the geometry point.
 *
 * Categories included (no wikidata filter — every OSM element with the tag
 * is returned, regardless of whether it has a Wikidata entry):
 *   tourism   = attraction / viewpoint / museum / artwork / theme_park /
 *               zoo / aquarium / gallery
 *   historic  = any historic=* (castle, monument, memorial, ruins,
 *               archaeological_site, shrine, tomb, wayside_shrine, …)
 *   amenity   = place_of_worship / theatre / arts_centre
 *   leisure   = park / garden / nature_reserve
 *   natural   = peak / volcano / waterfall
 *
 * Each feature carries the full set of useful OSM tags so the UI can show
 * complete info (EN/JA/local names, wikidata, wikipedia, commons, website,
 * phone, opening_hours, admission, operator, heritage/UNESCO, elevation,
 * address, description, image, etc.).
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export function osmPoiOverpassBody(bbox) {
  const lines = [
    // tourism
    `node["tourism"="attraction"](${bbox});`,
    `way["tourism"="attraction"](${bbox});`,
    `node["tourism"="museum"](${bbox});`,
    `way["tourism"="museum"](${bbox});`,
    `node["tourism"="viewpoint"](${bbox});`,
    `node["tourism"="artwork"](${bbox});`,
    `node["tourism"="theme_park"](${bbox});`,
    `way["tourism"="theme_park"](${bbox});`,
    `node["tourism"="zoo"](${bbox});`,
    `way["tourism"="zoo"](${bbox});`,
    `node["tourism"="aquarium"](${bbox});`,
    `way["tourism"="aquarium"](${bbox});`,
    `node["tourism"="gallery"](${bbox});`,

    // historic — any historic=*
    `node["historic"](${bbox});`,
    `way["historic"](${bbox});`,

    // amenity
    `node["amenity"="place_of_worship"](${bbox});`,
    `way["amenity"="place_of_worship"](${bbox});`,
    `node["amenity"="theatre"](${bbox});`,
    `node["amenity"="arts_centre"](${bbox});`,

    // leisure
    `node["leisure"="park"](${bbox});`,
    `way["leisure"="park"](${bbox});`,
    `node["leisure"="garden"](${bbox});`,
    `way["leisure"="garden"](${bbox});`,
    `node["leisure"="nature_reserve"](${bbox});`,
    `way["leisure"="nature_reserve"](${bbox});`,

    // natural
    `node["natural"="peak"](${bbox});`,
    `node["natural"="volcano"](${bbox});`,
    `node["natural"="waterfall"](${bbox});`,
  ];
  return lines.join('');
}

export function osmPoiCategoryOf(tags) {
  if (tags?.tourism) return tags.tourism;
  if (tags?.historic) return tags.historic;
  if (tags?.amenity === 'place_of_worship') {
    return tags.religion ? `place_of_worship:${tags.religion}` : 'place_of_worship';
  }
  if (tags?.amenity) return tags.amenity;
  if (tags?.leisure) return tags.leisure;
  if (tags?.natural) return tags.natural;
  return 'place';
}

function wikipediaUrl(tag) {
  if (!tag || typeof tag !== 'string') return null;
  const idx = tag.indexOf(':');
  if (idx === -1) return `https://en.wikipedia.org/wiki/${encodeURIComponent(tag.replace(/ /g, '_'))}`;
  const lang = tag.slice(0, idx);
  const title = tag.slice(idx + 1).replace(/ /g, '_');
  return `https://${lang}.wikipedia.org/wiki/${encodeURIComponent(title)}`;
}

function commonsUrl(tag) {
  if (!tag || typeof tag !== 'string') return null;
  const t = tag.startsWith('Category:') || tag.startsWith('File:') ? tag : `File:${tag}`;
  return `https://commons.wikimedia.org/wiki/${encodeURIComponent(t.replace(/ /g, '_'))}`;
}

/**
 * Map an Overpass element to a rich GeoJSON POI feature. Exported so other
 * collectors (e.g. googleMyMaps OSM fallback) can reuse the exact same shape.
 */
export function osmPoiMapFeature(el, i, coords) {
  const t = el.tags || {};
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: coords },
    properties: {
      id: `OSM_${el.type}_${el.id}`,
      platform: 'osm_popular_place',
      osm_type: el.type,
      osm_id: el.id,

      // Names
      name: t['name:en'] || t.name || `Place ${i + 1}`,
      name_ja: t.name || null,
      name_en: t['name:en'] || null,
      name_local: t.name || null,
      alt_name: t.alt_name || null,
      official_name: t.official_name || null,

      // Categorisation
      category: osmPoiCategoryOf(t),
      tourism: t.tourism || null,
      historic: t.historic || null,
      amenity: t.amenity || null,
      leisure: t.leisure || null,
      natural: t.natural || null,
      religion: t.religion || null,
      denomination: t.denomination || null,

      // Identifiers / links
      wikidata: t.wikidata || null,
      wikidata_url: t.wikidata ? `https://www.wikidata.org/wiki/${t.wikidata}` : null,
      wikipedia: t.wikipedia || null,
      wikipedia_url: wikipediaUrl(t.wikipedia),
      wikimedia_commons: t.wikimedia_commons || null,
      wikimedia_commons_url: commonsUrl(t.wikimedia_commons),
      website: t.website || t['contact:website'] || null,
      url: t.url || null,
      image: t.image || null,

      // Contact
      phone: t.phone || t['contact:phone'] || null,
      email: t.email || t['contact:email'] || null,

      // Practical info
      opening_hours: t.opening_hours || null,
      fee: t.fee || null,
      charge: t.charge || null,
      admission: t.admission || null,
      operator: t.operator || null,
      start_date: t.start_date || null,
      heritage: t.heritage || null,
      heritage_operator: t['heritage:operator'] || null,
      unesco: t.unesco || null,

      // Geography
      ele: t.ele || null,
      addr_full: t['addr:full'] || null,
      addr_city: t['addr:city'] || null,
      addr_prefecture: t['addr:province'] || t['addr:state'] || null,
      addr_postcode: t['addr:postcode'] || null,

      // Freeform
      description: t.description || t['description:en'] || null,

      source: 'osm_overpass',
    },
  };
}

export default async function collectFamousPlaces() {
  const features = await fetchOverpassTiled(
    osmPoiOverpassBody,
    osmPoiMapFeature,
    { queryTimeout: 180, timeoutMs: 90_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'famous-places',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      live_source: list.length > 0 ? 'osm_overpass' : null,
      description: 'Famous places & noteworthy OSM POIs across Japan — tourism/historic/worship/leisure/natural (nationwide tiled Overpass, full metadata)',
    },
    metadata: {},
  };
}
