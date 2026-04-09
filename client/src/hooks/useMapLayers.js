import { useState, useCallback, useRef } from 'react';

const LAYER_DEFINITIONS = {
  earthquakes: {
    name: 'Earthquakes',
    icon: '\u{1F534}',
    color: '#ff4444',
    endpoint: '/api/data/earthquake',
    category: 'Environment',
  },
  weather: {
    name: 'Weather',
    icon: '\u{1F324}',
    color: '#4fc3f7',
    endpoint: '/api/data/weather',
    category: 'Environment',
  },
  transport: {
    name: 'Transit Stations',
    icon: '\u{1F683}',
    color: '#66bb6a',
    endpoint: '/api/data/transport',
    category: 'Transport',
  },
  airQuality: {
    name: 'Air Quality',
    icon: '\u{1F4A8}',
    color: '#ffb74d',
    endpoint: '/api/data/air-quality',
    category: 'Environment',
  },
  radiation: {
    name: 'Radiation',
    icon: '\u{2622}',
    color: '#ffd600',
    endpoint: '/api/data/radiation',
    category: 'Safety',
  },
  cameras: {
    name: 'Cameras',
    icon: '\u{1F4F7}',
    color: '#ce93d8',
    endpoint: '/api/data/cameras',
    category: 'Infrastructure',
  },
  population: {
    name: 'Population',
    icon: '\u{1F465}',
    color: '#4dd0e1',
    endpoint: '/api/data/population',
    category: 'Social',
  },
  landPrice: {
    name: 'Land Prices',
    icon: '\u{1F4B0}',
    color: '#aed581',
    endpoint: '/api/data/landprice',
    category: 'Economy',
  },
  river: {
    name: 'River Levels',
    icon: '\u{1F30A}',
    color: '#42a5f5',
    endpoint: '/api/data/river',
    category: 'Environment',
  },
  crime: {
    name: 'Crime/Police',
    icon: '\u{1F694}',
    color: '#ef5350',
    endpoint: '/api/data/crime',
    category: 'Safety',
  },
  buildings: {
    name: 'Buildings',
    icon: '\u{1F3E2}',
    color: '#78909c',
    endpoint: '/api/data/buildings',
    category: 'Infrastructure',
  },
  social: {
    name: 'Social Media',
    icon: '\u{1F4AC}',
    color: '#f06292',
    endpoint: '/api/data/social',
    category: 'Social',
  },

  // ── Social Media Geo (expanded) ─────────────────────────────────
  twitterGeo: {
    name: 'Twitter/X Geo',
    icon: '\u{1F426}',
    color: '#1da1f2',
    endpoint: '/api/data/twitter-geo',
    category: 'Social',
  },
  facebookGeo: {
    name: 'Facebook Check-ins',
    icon: '\u{1F4D8}',
    color: '#4267b2',
    endpoint: '/api/data/facebook-geo',
    category: 'Social',
  },
  snapchatHeatmap: {
    name: 'Snapchat Heatmap',
    icon: '\u{1F47B}',
    color: '#fffc00',
    endpoint: '/api/data/snapchat-heatmap',
    category: 'Social',
  },

  // ── Marketplace / Classifieds ───────────────────────────────────
  classifieds: {
    name: 'Classifieds (Jmty)',
    icon: '\u{1F4DD}',
    color: '#ff9800',
    endpoint: '/api/data/classifieds',
    category: 'Marketplace',
  },
  realEstate: {
    name: 'Real Estate (Suumo)',
    icon: '\u{1F3D8}',
    color: '#8bc34a',
    endpoint: '/api/data/real-estate',
    category: 'Marketplace',
  },
  jobBoards: {
    name: 'Job Boards',
    icon: '\u{1F4BC}',
    color: '#a1887f',
    endpoint: '/api/data/job-boards',
    category: 'Marketplace',
  },

  // ── Cyber OSINT ─────────────────────────────────────────────────
  googleDorking: {
    name: 'Google Dorks',
    icon: '\u{1F50D}',
    color: '#4285f4',
    endpoint: '/api/data/google-dorking',
    category: 'Cyber',
  },
  shodanIot: {
    name: 'Shodan IoT',
    icon: '\u{1F4E1}',
    color: '#d62d20',
    endpoint: '/api/data/shodan-iot',
    category: 'Cyber',
  },
  insecamWebcams: {
    name: 'Open Webcams',
    icon: '\u{1F3A5}',
    color: '#e91e63',
    endpoint: '/api/data/insecam-webcams',
    category: 'Cyber',
  },
  wifiNetworks: {
    name: 'WiFi Networks',
    icon: '\u{1F4F6}',
    color: '#00bcd4',
    endpoint: '/api/data/wifi-networks',
    category: 'Cyber',
  },

  // ── Transport (nationwide) ──────────────────────────────────────
  fullTransport: {
    name: 'Japan Rail Network',
    icon: '\u{1F684}',
    color: '#43a047',
    endpoint: '/api/data/full-transport',
    category: 'Transport',
  },
  busRoutes: {
    name: 'Bus Terminals',
    icon: '\u{1F68C}',
    color: '#fb8c00',
    endpoint: '/api/data/bus-routes',
    category: 'Transport',
  },
  ferryRoutes: {
    name: 'Ferry Terminals',
    icon: '\u{26F4}',
    color: '#039be5',
    endpoint: '/api/data/ferry-routes',
    category: 'Transport',
  },
  highwayTraffic: {
    name: 'Expressway IC/JCT',
    icon: '\u{1F6E3}',
    color: '#9e9e9e',
    endpoint: '/api/data/highway-traffic',
    category: 'Transport',
  },
  maritimeAis: {
    name: 'AIS Ship Tracking',
    icon: '\u{1F6A2}',
    color: '#0277bd',
    endpoint: '/api/data/maritime-ais',
    category: 'Transport',
  },
  flightAdsb: {
    name: 'ADS-B Flights',
    icon: '\u{2708}',
    color: '#7c4dff',
    endpoint: '/api/data/flight-adsb',
    category: 'Transport',
  },

  // ── Infrastructure ──────────────────────────────────────────────
  electricalGrid: {
    name: 'Electrical Grid',
    icon: '\u{26A1}',
    color: '#ffeb3b',
    endpoint: '/api/data/electrical-grid',
    category: 'Infrastructure',
  },
  gasNetwork: {
    name: 'Gas Network',
    icon: '\u{1F525}',
    color: '#ff5722',
    endpoint: '/api/data/gas-network',
    category: 'Infrastructure',
  },
  waterInfra: {
    name: 'Water Infrastructure',
    icon: '\u{1F4A7}',
    color: '#29b6f6',
    endpoint: '/api/data/water-infra',
    category: 'Infrastructure',
  },
  cellTowers: {
    name: 'Cell Towers',
    icon: '\u{1F4F1}',
    color: '#9c27b0',
    endpoint: '/api/data/cell-towers',
    category: 'Infrastructure',
  },
  nuclearFacilities: {
    name: 'Nuclear Facilities',
    icon: '\u{2622}',
    color: '#76ff03',
    endpoint: '/api/data/nuclear-facilities',
    category: 'Infrastructure',
  },
};

export const LAYER_CATEGORIES = [
  'Environment',
  'Transport',
  'Safety',
  'Economy',
  'Social',
  'Marketplace',
  'Cyber',
  'Infrastructure',
];

export { LAYER_DEFINITIONS };

export default function useMapLayers() {
  const [layers, setLayers] = useState(() => {
    const initial = {};
    for (const key of Object.keys(LAYER_DEFINITIONS)) {
      initial[key] = { visible: false, opacity: 1, loading: false };
    }
    return initial;
  });

  const [layerData, setLayerData] = useState({});
  const [isLoading, setIsLoading] = useState(false);
  const cacheRef = useRef({});

  const fetchLayerData = useCallback(async (layerId) => {
    const def = LAYER_DEFINITIONS[layerId];
    if (!def) return;

    if (cacheRef.current[layerId]) {
      setLayerData((prev) => ({ ...prev, [layerId]: cacheRef.current[layerId] }));
      return;
    }

    setLayers((prev) => ({
      ...prev,
      [layerId]: { ...prev[layerId], loading: true },
    }));

    try {
      const res = await fetch(def.endpoint);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();

      const geojson = data.type === 'FeatureCollection' ? data : {
        type: 'FeatureCollection',
        features: Array.isArray(data) ? data : (data.features || []),
      };

      cacheRef.current[layerId] = geojson;
      setLayerData((prev) => ({ ...prev, [layerId]: geojson }));
    } catch (err) {
      console.warn(`[useMapLayers] Failed to fetch ${layerId}:`, err.message);
      setLayerData((prev) => ({
        ...prev,
        [layerId]: { type: 'FeatureCollection', features: [] },
      }));
    } finally {
      setLayers((prev) => ({
        ...prev,
        [layerId]: { ...prev[layerId], loading: false },
      }));
    }
  }, []);

  const toggleLayer = useCallback((layerId) => {
    setLayers((prev) => {
      const current = prev[layerId];
      if (!current) return prev;
      const newVisible = !current.visible;

      if (newVisible && !cacheRef.current[layerId]) {
        fetchLayerData(layerId);
      }

      return {
        ...prev,
        [layerId]: { ...current, visible: newVisible },
      };
    });
  }, [fetchLayerData]);

  const setLayerOpacity = useCallback((layerId, opacity) => {
    setLayers((prev) => ({
      ...prev,
      [layerId]: { ...prev[layerId], opacity },
    }));
  }, []);

  const setAllLayers = useCallback((visible) => {
    setLayers((prev) => {
      const updated = {};
      for (const key of Object.keys(prev)) {
        updated[key] = { ...prev[key], visible };
        if (visible && !cacheRef.current[key]) {
          fetchLayerData(key);
        }
      }
      return updated;
    });
  }, [fetchLayerData]);

  const refreshLayer = useCallback((layerId) => {
    delete cacheRef.current[layerId];
    fetchLayerData(layerId);
  }, [fetchLayerData]);

  const activeCount = Object.values(layers).filter((l) => l.visible).length;

  return {
    layers,
    layerDefinitions: LAYER_DEFINITIONS,
    toggleLayer,
    setLayerOpacity,
    setAllLayers,
    refreshLayer,
    layerData,
    isLoading,
    activeCount,
  };
}
