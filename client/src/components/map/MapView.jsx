import React, { useRef, useEffect, useState, useCallback } from 'react';
import maplibregl from 'maplibre-gl';
import 'maplibre-gl/dist/maplibre-gl.css';
import { LAYER_DEFINITIONS } from '../../hooks/useMapLayers';

const MAP_STYLES = {
  gsi_pale: {
    version: 8,
    name: 'GSI Pale (Dark)',
    sources: {
      gsi: {
        type: 'raster',
        tiles: ['https://cyberjapandata.gsi.go.jp/xyz/pale/{z}/{x}/{y}.png'],
        tileSize: 256,
        attribution: '<a href="https://maps.gsi.go.jp/" target="_blank">GSI Japan</a>',
        maxzoom: 18,
      },
    },
    layers: [
      {
        id: 'gsi-tiles',
        type: 'raster',
        source: 'gsi',
        paint: {
          'raster-saturation': -0.5,
          'raster-brightness-max': 0.4,
          'raster-contrast': 0.2,
        },
      },
    ],
  },
  osm_dark: {
    version: 8,
    name: 'OSM Dark',
    sources: {
      osm: {
        type: 'raster',
        tiles: ['https://tile.openstreetmap.org/{z}/{x}/{y}.png'],
        tileSize: 256,
        attribution: '&copy; OpenStreetMap contributors',
        maxzoom: 19,
      },
    },
    layers: [
      {
        id: 'osm-tiles',
        type: 'raster',
        source: 'osm',
        paint: {
          'raster-saturation': -0.8,
          'raster-brightness-max': 0.35,
          'raster-contrast': 0.3,
        },
      },
    ],
  },
};

function addLayerToMap(map, layerId, geojson, layerDef, opacity) {
  const sourceId = `source-${layerId}`;
  const mainLayerId = `layer-${layerId}`;

  // Remove existing
  if (map.getLayer(mainLayerId)) map.removeLayer(mainLayerId);
  if (map.getLayer(`${mainLayerId}-heat`)) map.removeLayer(`${mainLayerId}-heat`);
  if (map.getLayer(`${mainLayerId}-extrude`)) map.removeLayer(`${mainLayerId}-extrude`);
  if (map.getLayer(`${mainLayerId}-line`)) map.removeLayer(`${mainLayerId}-line`);
  if (map.getSource(sourceId)) map.removeSource(sourceId);

  if (!geojson || !geojson.features || geojson.features.length === 0) return;

  map.addSource(sourceId, { type: 'geojson', data: geojson });

  switch (layerId) {
    case 'earthquakes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'magnitude'], ['get', 'mag'], 3],
            1, 4,
            3, 8,
            5, 16,
            7, 28,
            9, 40,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'magnitude'], ['get', 'mag'], 3],
            1, '#ffeb3b',
            3, '#ff9800',
            5, '#ff5722',
            7, '#f44336',
            9, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ff4444',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'weather':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 8,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'transport':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'airQuality':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 10,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'aqi'], ['get', 'value'], 50],
            0, '#00ff88',
            50, '#ffeb3b',
            100, '#ff9800',
            150, '#ff4444',
            300, '#8b0000',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#000000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'radiation':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 8,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'value'], ['get', 'nGy'], 30],
            0, '#00ff88',
            50, '#ffd600',
            100, '#ff8c00',
            200, '#ff4444',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffd600',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'cameras':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'population':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'population'], ['get', 'density'], ['get', 'value'], 1000],
            0, 0,
            100000, 1,
          ],
          'heatmap-intensity': 1,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#0d47a1',
            0.4, '#1565c0',
            0.6, '#00bcd4',
            0.8, '#4dd0e1',
            1, '#e0f7fa',
          ],
          'heatmap-radius': 30,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      break;

    case 'landPrice':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'price'], ['get', 'value'], 100000],
            10000, '#2196f3',
            50000, '#4caf50',
            100000, '#ffeb3b',
            500000, '#ff9800',
            1000000, '#f44336',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'river':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 7,
          'circle-color': '#42a5f5',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#1565c0',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'crime':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': ['coalesce', ['get', 'count'], ['get', 'incidents'], 1],
          'heatmap-intensity': 1.5,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#311b92',
            0.4, '#d32f2f',
            0.6, '#ff5722',
            0.8, '#ff9800',
            1, '#ffeb3b',
          ],
          'heatmap-radius': 25,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      break;

    case 'buildings':
      map.addLayer({
        id: `${mainLayerId}-extrude`,
        type: 'fill-extrusion',
        source: sourceId,
        paint: {
          'fill-extrusion-color': layerDef.color,
          'fill-extrusion-height': ['coalesce', ['get', 'height'], 20],
          'fill-extrusion-base': 0,
          'fill-extrusion-opacity': opacity * 0.6,
        },
      });
      break;

    case 'social':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'platform'], 'other'],
            'twitter', '#1da1f2',
            'mastodon', '#6364ff',
            'reddit', '#ff4500',
            'youtube', '#ff0000',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.2,
        },
      });
      break;

    default:
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.8,
        },
      });
  }
}

function removeLayerFromMap(map, layerId) {
  const mainLayerId = `layer-${layerId}`;
  const sourceId = `source-${layerId}`;
  const variants = [mainLayerId, `${mainLayerId}-heat`, `${mainLayerId}-extrude`, `${mainLayerId}-line`];

  for (const lid of variants) {
    if (map.getLayer(lid)) map.removeLayer(lid);
  }
  if (map.getSource(sourceId)) map.removeSource(sourceId);
}

export default function MapView({ layers, layerData, onFeatureClick }) {
  const mapContainerRef = useRef(null);
  const mapRef = useRef(null);
  const [mapReady, setMapReady] = useState(false);
  const [cursorCoords, setCursorCoords] = useState(null);
  const [zoom, setZoom] = useState(5);
  const [currentStyle, setCurrentStyle] = useState('gsi_pale');
  const prevLayersRef = useRef({});

  // Initialize map
  useEffect(() => {
    if (mapRef.current || !mapContainerRef.current) return;

    const map = new maplibregl.Map({
      container: mapContainerRef.current,
      style: MAP_STYLES[currentStyle],
      center: [138, 36.5],
      zoom: 5,
      maxBounds: [[120, 20], [155, 50]],
      attributionControl: true,
    });

    map.addControl(new maplibregl.NavigationControl({ showCompass: true }), 'top-right');
    map.addControl(new maplibregl.ScaleControl({ maxWidth: 150 }), 'bottom-right');

    map.on('load', () => {
      setMapReady(true);
    });

    map.on('mousemove', (e) => {
      setCursorCoords({ lat: e.lngLat.lat.toFixed(5), lng: e.lngLat.lng.toFixed(5) });

      // Change cursor on hover over interactive features
      const interactiveLayers = [];
      for (const layerId of Object.keys(LAYER_DEFINITIONS)) {
        const mainId = `layer-${layerId}`;
        if (map.getLayer(mainId)) interactiveLayers.push(mainId);
      }
      if (interactiveLayers.length > 0) {
        const features = map.queryRenderedFeatures(e.point, { layers: interactiveLayers });
        map.getCanvas().style.cursor = features.length > 0 ? 'pointer' : '';
      }
    });

    map.on('zoom', () => {
      setZoom(map.getZoom().toFixed(1));
    });

    map.on('click', (e) => {
      // Check all interactive layers
      const interactiveLayers = [];
      for (const layerId of Object.keys(LAYER_DEFINITIONS)) {
        const mainId = `layer-${layerId}`;
        if (map.getLayer(mainId)) interactiveLayers.push(mainId);
      }

      if (interactiveLayers.length === 0) return;

      const features = map.queryRenderedFeatures(e.point, { layers: interactiveLayers });
      if (features.length > 0) {
        const feature = features[0];
        const layerType = feature.layer.id.replace('layer-', '');
        if (onFeatureClick) {
          onFeatureClick(feature, layerType, { x: e.point.x, y: e.point.y });
        }
      }
    });

    mapRef.current = map;

    return () => {
      map.remove();
      mapRef.current = null;
    };
  }, []);

  // Sync layers
  useEffect(() => {
    if (!mapReady || !mapRef.current) return;
    const map = mapRef.current;

    for (const [layerId, layerState] of Object.entries(layers)) {
      const def = LAYER_DEFINITIONS[layerId];
      if (!def) continue;

      const data = layerData[layerId];
      const prev = prevLayersRef.current[layerId];
      const wasVisible = prev?.visible;
      const wasData = prev?.dataLength;
      const currentDataLength = data?.features?.length ?? 0;

      if (layerState.visible && data) {
        if (!wasVisible || wasData !== currentDataLength) {
          addLayerToMap(map, layerId, data, def, layerState.opacity);
        }
      } else if (!layerState.visible && wasVisible) {
        removeLayerFromMap(map, layerId);
      }

      prevLayersRef.current[layerId] = {
        visible: layerState.visible,
        dataLength: currentDataLength,
      };
    }
  }, [layers, layerData, mapReady]);

  // Style switcher
  const switchStyle = useCallback((styleKey) => {
    if (!mapRef.current || styleKey === currentStyle) return;
    mapRef.current.setStyle(MAP_STYLES[styleKey]);
    setCurrentStyle(styleKey);
    prevLayersRef.current = {};

    // Re-add layers after style change
    mapRef.current.once('style.load', () => {
      for (const [layerId, layerState] of Object.entries(layers)) {
        if (layerState.visible && layerData[layerId]) {
          addLayerToMap(
            mapRef.current,
            layerId,
            layerData[layerId],
            LAYER_DEFINITIONS[layerId],
            layerState.opacity
          );
        }
      }
    });
  }, [currentStyle, layers, layerData]);

  return (
    <div className="map-container">
      <div ref={mapContainerRef} className="map-wrapper" />

      {/* Style switcher */}
      <div className="absolute top-3 right-14 z-20 flex gap-1">
        {Object.entries(MAP_STYLES).map(([key, style]) => (
          <button
            key={key}
            onClick={() => switchStyle(key)}
            className={`px-2 py-1 text-[10px] rounded border transition-colors ${
              currentStyle === key
                ? 'border-neon-cyan/50 bg-neon-cyan/10 text-neon-cyan'
                : 'border-osint-border bg-osint-surface/80 text-gray-500 hover:text-gray-300'
            }`}
          >
            {style.name}
          </button>
        ))}
      </div>

      {/* Cursor coordinates */}
      {cursorCoords && (
        <div className="absolute bottom-1 left-1 z-20 text-[10px] font-mono text-gray-500 bg-osint-bg/80 px-2 py-0.5 rounded">
          {cursorCoords.lat}, {cursorCoords.lng} | z{zoom}
        </div>
      )}
    </div>
  );
}
