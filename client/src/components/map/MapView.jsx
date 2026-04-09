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

    // ── Twitter/X geocoded posts ───────────────────────────────
    case 'twitterGeo':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'engagement'], ['get', 'likes'], 1],
            0, 3,
            10, 5,
            100, 8,
            1000, 14,
            10000, 22,
          ],
          'circle-color': '#1da1f2',
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Facebook check-ins ─────────────────────────────────────
    case 'facebookGeo':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'checkins'], 1],
            0, 4,
            100, 8,
            1000, 14,
            10000, 22,
          ],
          'circle-color': '#4267b2',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Snapchat heatmap ───────────────────────────────────────
    case 'snapchatHeatmap':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'activity'], ['get', 'snaps'], 1],
            0, 0,
            100, 1,
          ],
          'heatmap-intensity': 1.4,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#332b00',
            0.4, '#806d00',
            0.6, '#ccae00',
            0.8, '#ffd700',
            1, '#fffc00',
          ],
          'heatmap-radius': 30,
          'heatmap-opacity': opacity * 0.75,
        },
      });
      break;

    // ── Marketplace: classifieds ───────────────────────────────
    case 'classifieds':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'category'], 'other'],
            'free_items', '#4caf50',
            'barter', '#ff9800',
            'jobs', '#2196f3',
            'services', '#9c27b0',
            'vehicles', '#f44336',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Real estate ────────────────────────────────────────────
    case 'realEstate':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'price_jpy'], ['get', 'rent_jpy'], 50000],
            0, 3,
            50000, 5,
            200000, 9,
            1000000, 14,
            50000000, 20,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'listing_type'], 'rent'],
            'rent', '#8bc34a',
            'sale', '#ff7043',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Job boards ─────────────────────────────────────────────
    case 'jobBoards':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 3.5,
          'circle-color': [
            'match', ['coalesce', ['get', 'employment_type'], 'other'],
            'part_time', '#a1887f',
            'full_time', '#6d4c41',
            'temporary', '#bcaaa4',
            'gig', '#ff7043',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.2,
        },
      });
      break;

    // ── Google dorking results ─────────────────────────────────
    case 'googleDorking':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': [
            'match', ['coalesce', ['get', 'dork_category'], 'other'],
            'exposed_admin', '#f44336',
            'exposed_files', '#ff9800',
            'open_directories', '#ffeb3b',
            'cameras', '#e91e63',
            'iot_devices', '#9c27b0',
            'government', '#2196f3',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Shodan IoT devices ─────────────────────────────────────
    case 'shodanIot':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'device_type'], 'other'],
            'ip_camera', '#e91e63',
            'router', '#03a9f4',
            'nas', '#4caf50',
            'scada', '#f44336',
            'plc', '#ff5722',
            'printer', '#9e9e9e',
            'database', '#9c27b0',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Open webcams ───────────────────────────────────────────
    case 'insecamWebcams':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': '#e91e63',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── WiFi networks ──────────────────────────────────────────
    case 'wifiNetworks':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': 0.5,
          'heatmap-intensity': 1.2,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#003355',
            0.4, '#005577',
            0.6, '#0077aa',
            0.8, '#00aacc',
            1, '#00ffff',
          ],
          'heatmap-radius': 18,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 2,
          'circle-color': '#00bcd4',
          'circle-opacity': opacity * 0.6,
        },
      });
      break;

    // ── Maritime AIS ───────────────────────────────────────────
    case 'maritimeAis':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'length_m'], 50],
            0, 3,
            50, 5,
            150, 8,
            300, 12,
            400, 16,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'vessel_type'], 'other'],
            'cargo', '#0277bd',
            'tanker', '#ef6c00',
            'fishing', '#558b2f',
            'passenger', '#7c4dff',
            'military', '#c62828',
            'tug', '#5d4037',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Flight ADS-B ───────────────────────────────────────────
    case 'flightAdsb':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'altitude_ft'], 0],
            0, 3,
            10000, 5,
            30000, 8,
            40000, 11,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'altitude_ft'], 0],
            0, '#7c4dff',
            10000, '#536dfe',
            20000, '#448aff',
            30000, '#40c4ff',
            40000, '#18ffff',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Full transport (nationwide rail) ───────────────────────
    case 'fullTransport':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'station_type'], 'other'],
            'shinkansen', 7,
            'jr_major', 5,
            'subway', 3.5,
            'private', 3,
            4,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'station_type'], 'other'],
            'shinkansen', '#e53935',
            'jr_major', '#43a047',
            'subway', '#1e88e5',
            'private', '#fb8c00',
            'monorail', '#8e24aa',
            'tram', '#00897b',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Bus terminals ──────────────────────────────────────────
    case 'busRoutes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': '#fb8c00',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Ferry terminals ────────────────────────────────────────
    case 'ferryRoutes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': '#039be5',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Highway IC/JCT/SA/PA ───────────────────────────────────
    case 'highwayTraffic':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'facility_type'], 'IC'],
            'JCT', 7,
            'SA', 6,
            'PA', 4,
            'IC', 5,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'congestion'], 'free'],
            'jam', '#f44336',
            'congested', '#ff9800',
            'slow', '#ffeb3b',
            '#4caf50',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Electrical grid ────────────────────────────────────────
    case 'electricalGrid':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mw'], 0],
            0, 4,
            500, 6,
            1500, 9,
            3000, 13,
            6000, 18,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'thermal', '#ff5722',
            'hydro', '#03a9f4',
            'hydro_pumped', '#0288d1',
            'geothermal', '#d84315',
            'wind', '#80deea',
            'solar', '#ffd54f',
            'substation', '#9e9e9e',
            'frequency_converter', '#ab47bc',
            'hvdc_converter', '#7e57c2',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Gas network ────────────────────────────────────────────
    case 'gasNetwork':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_kt'], 0],
            0, 4,
            300, 7,
            1000, 11,
            3000, 16,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'lng_terminal', '#ff5722',
            'distribution', '#ff9800',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Water infrastructure ───────────────────────────────────
    case 'waterInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mcm'], 0],
            0, 4,
            10, 6,
            100, 9,
            500, 13,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'dam', '#1e88e5',
            'water_treatment', '#29b6f6',
            'sewage', '#7e57c2',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Cell towers ────────────────────────────────────────────
    case 'cellTowers':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': 0.4,
          'heatmap-intensity': 1.1,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#1a0033',
            0.4, '#330066',
            0.6, '#660099',
            0.8, '#9c27b0',
            1, '#e040fb',
          ],
          'heatmap-radius': 14,
          'heatmap-opacity': opacity * 0.6,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 2.5,
          'circle-color': [
            'match', ['coalesce', ['get', 'carrier'], 'other'],
            'NTT Docomo', '#d32f2f',
            'au by KDDI', '#fb8c00',
            'SoftBank', '#9e9e9e',
            'Rakuten Mobile', '#bf360c',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.3,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Nuclear facilities ─────────────────────────────────────
    case 'nuclearFacilities':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mw'], 0],
            0, 6,
            1000, 10,
            3000, 14,
            8000, 20,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'status'], 'other'],
            'active', '#76ff03',
            'restart_approved', '#cddc39',
            'restart_pending', '#ffeb3b',
            'suspended', '#ff9800',
            'decommissioning', '#9e9e9e',
            'commissioning', '#80d8ff',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffd600',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    // ── Wave 1: Public Safety + Disaster ──────────────────────────
    case 'hospitalMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'beds'], 200],
            0, 4,
            300, 6,
            600, 9,
            1000, 13,
            1500, 18,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'hospital_type'], 'general'],
            'university', '#e53935',
            'national', '#ad1457',
            'specialized', '#f06292',
            'public', '#fb8c00',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'aedMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': [
            'match', ['coalesce', ['get', 'location_type'], 'public'],
            'station', '#ff5252',
            'airport', '#ff8a80',
            'stadium', '#ff1744',
            'tourist', '#f06292',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'kobanMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'police_type'], 'station'],
            'headquarters', 12,
            'station', 7,
            'koban', 4,
            'chuzaisho', 4,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'police_type'], 'station'],
            'headquarters', '#0d47a1',
            'station', '#1565c0',
            'koban', '#42a5f5',
            'chuzaisho', '#90caf9',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'fireStationMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'station_type'], 'station'],
            'headquarters', 11,
            'station', 6,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'station_type'], 'station'],
            'headquarters', '#bf360c',
            'station', '#d84315',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'bosaiShelter':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity'], 1000],
            0, 4,
            5000, 7,
            20000, 11,
            60000, 16,
            100000, 22,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'shelter_type'], 'designated'],
            'evacuation_area', '#00897b',
            'designated', '#26a69a',
            'tsunami_tower', '#0277bd',
            'tsunami_building', '#039be5',
            'assembly_point', '#4db6ac',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'hazardMapPortal':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'max_depth_m'], ['get', 'alert_level'], 5],
            0, 6,
            5, 10,
            15, 16,
            30, 22,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'hazard_type'], 'flood'],
            'tsunami', '#0277bd',
            'volcano', '#bf360c',
            'landslide', '#6d4c41',
            'flood', '#0288d1',
            'liquefaction', '#8d6e63',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'jshisSeismic':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        maxzoom: 15,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0.1],
            0, 0,
            0.5, 0.7,
            1, 1,
          ],
          'heatmap-intensity': ['interpolate', ['linear'], ['zoom'], 0, 1, 12, 3],
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(33, 102, 172, 0)',
            0.2, '#3288bd',
            0.4, '#fee08b',
            0.6, '#f46d43',
            0.8, '#d73027',
            1, '#a50026',
          ],
          'heatmap-radius': ['interpolate', ['linear'], ['zoom'], 0, 20, 12, 60],
          'heatmap-opacity': opacity * 0.7,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        minzoom: 7,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0],
            0, 4,
            0.3, 7,
            0.6, 12,
            0.9, 18,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0],
            0, '#3288bd',
            0.3, '#fee08b',
            0.6, '#f46d43',
            0.9, '#a50026',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'hiNet':
    case 'kNet':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'jmaIntensity':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'intensity_numeric'], ['get', 'magnitude'], 5],
            0, 5,
            4, 9,
            6, 16,
            7, 24,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'intensity_numeric'], ['get', 'magnitude'], 5],
            0, '#ffeb3b',
            3, '#ff9800',
            5, '#ff5722',
            6, '#d84315',
            7, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Wave 2: Health + Statistics + Commerce ─────────────────────
    case 'pharmacyMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': '#26a69a',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'convenienceStores':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['get', 'brand'],
            '7-Eleven', '#ff6b35',
            'FamilyMart', '#1a73e8',
            'Lawson', '#0077c8',
            'MiniStop', '#f1c232',
            'NewDays', '#43a047',
            '#43a047',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'gasStations':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': [
            'match', ['get', 'brand'],
            'ENEOS', '#e60012',
            'Idemitsu', '#ee7800',
            'Cosmo', '#003da5',
            'Shell', '#fbce07',
            'JA-SS', '#43a047',
            '#e64a19',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'tabelogRestaurants':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'rating'], 3.5],
            3.0, 4,
            4.0, 7,
            4.5, 10,
            5.0, 14,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'rating'], 3.5],
            3.0, '#ffcc80',
            4.0, '#fb8c00',
            4.5, '#e65100',
            5.0, '#bf360c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'estatCensus':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'population'], 1000000],
            500000, 6,
            1000000, 10,
            5000000, 18,
            14000000, 30,
          ],
          'circle-color': '#5e35b1',
          'circle-opacity': opacity * 0.55,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#b39ddb',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'resasPopulation':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'population'], 500000],
            100000, 5,
            500000, 9,
            1500000, 15,
            3700000, 24,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'age_65plus_pct'], 25],
            15, '#3949ab',
            25, '#7e57c2',
            35, '#e91e63',
            45, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'resasTourism':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'visitors_yr'], 1000000],
            500000, 4,
            5000000, 9,
            15000000, 16,
            100000000, 28,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'foreign_pct'], 20],
            5, '#80deea',
            25, '#00acc1',
            50, '#006064',
            70, '#311b92',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'resasIndustry':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'employees'], 100000],
            10000, 5,
            100000, 10,
            500000, 18,
            4500000, 30,
          ],
          'circle-color': [
            'match', ['get', 'primary_sector'],
            'manufacturing', '#6d4c41',
            'services', '#1e88e5',
            'agriculture', '#7cb342',
            'fishery', '#0097a7',
            '#9e9e9e',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'mlitTransaction':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'transactions_q'], 200],
            50, 4,
            200, 8,
            300, 12,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'avg_price_yen_per_m2'], 1500000],
            500000, '#a5d6a7',
            1500000, '#66bb6a',
            3500000, '#fb8c00',
            6500000, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'damWaterLevel':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_m3'], 50000000],
            10000000, 5,
            100000000, 10,
            300000000, 16,
            660000000, 24,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'current_pct'], 70],
            0, '#b71c1c',
            40, '#fb8c00',
            70, '#42a5f5',
            90, '#0277bd',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
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
