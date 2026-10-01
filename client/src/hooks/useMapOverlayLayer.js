import { useEffect } from 'react';

/**
 * Keep one GeoJSON source + its style layers mounted on a MapLibre map for
 * the life of the calling component — and re-mount them after a base-style
 * switch, which drops every source and layer the style did not ship.
 *
 *   useMapOverlayLayer(mapRef, 'jo-aoi', geojson, [
 *     { id: 'jo-aoi-fill', type: 'fill', paint: {...} },
 *     { id: 'jo-aoi-line', type: 'line', paint: {...} },
 *   ]);
 *
 * `geojson` null/undefined removes the overlay. Layer specs must not carry
 * `source` — it is filled in. Layers are appended on top of the style.
 */
export default function useMapOverlayLayer(mapRef, sourceId, geojson, layerSpecs) {
  useEffect(() => {
    const map = mapRef?.current;
    if (!map) return undefined;

    const empty = { type: 'FeatureCollection', features: [] };
    const data = geojson || empty;

    const mount = () => {
      if (!map.getStyle?.()) return;
      try {
        if (!map.getSource(sourceId)) {
          map.addSource(sourceId, { type: 'geojson', data });
        } else {
          map.getSource(sourceId).setData(data);
        }
        for (const spec of layerSpecs || []) {
          if (!map.getLayer(spec.id)) {
            map.addLayer({ ...spec, source: sourceId });
          }
        }
      } catch (e) {
        // A style mid-load throws on addSource; the styledata handler below
        // retries once the style has settled.
        if (import.meta.env?.DEV) console.debug('[overlay] mount deferred:', e?.message);
      }
    };

    const onStyle = () => { if (map.isStyleLoaded?.()) mount(); };

    if (map.isStyleLoaded?.()) mount();
    else map.once('load', mount);
    map.on('styledata', onStyle);

    return () => {
      map.off('styledata', onStyle);
      map.off('load', mount);
      try {
        for (const spec of layerSpecs || []) {
          if (map.getLayer(spec.id)) map.removeLayer(spec.id);
        }
        if (map.getSource(sourceId)) map.removeSource(sourceId);
      } catch { /* map torn down */ }
    };
    // layerSpecs are treated as static per mount; geojson identity drives updates.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mapRef, mapRef?.current, sourceId, geojson]);
}
