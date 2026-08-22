import React, { useState, useCallback, useEffect, useRef } from 'react';
import { MdSearch } from 'react-icons/md';
import MapView from './MapView';
import LayerPanel from './LayerPanel';
import MapPopup from './MapPopup';
import useMapLayers from '../../hooks/useMapLayers';
import useMapProjection from '../../hooks/useMapProjection';
import useCameraDiscoveryStream from '../../hooks/useCameraDiscoveryStream';
import apiUrl from '../../utils/apiUrl.js';

export default function MapPage() {
  const {
    layers,
    toggleLayer,
    setLayerOpacity,
    setLayerTemporalWindow,
    setAllLayers,
    layerData,
    layerDataView,
    activeCount,
  } = useMapLayers();

  const { activeRun: cameraActiveRun } = useCameraDiscoveryStream();

  const camerasVisible = layers.cameras?.visible;
  const cameraTriggerFiredRef = useRef(false);
  useEffect(() => {
    if (!camerasVisible) {
      cameraTriggerFiredRef.current = false;
      return;
    }
    if (cameraTriggerFiredRef.current) return undefined;
    cameraTriggerFiredRef.current = true;
    const ctrl = new AbortController();
    let settled = false;
    fetch(apiUrl('/api/data/cameras/trigger'), { method: 'POST', signal: ctrl.signal })
      .catch((err) => {
        if (err?.name !== 'AbortError') {
          console.warn('[MapPage] camera trigger failed:', err?.message);
        }
      })
      .finally(() => { settled = true; });
    return () => {
      ctrl.abort();
      // Refs survive StrictMode's mount/cleanup/remount, so an aborted
      // in-flight POST must un-latch the guard or the remount suppresses
      // the trigger permanently in dev.
      if (!settled) cameraTriggerFiredRef.current = false;
    };
  }, [camerasVisible]);

  // Camera Discovery panel's "View on map" with no filters active asks the
  // map to surface the global Cameras layer if it's currently hidden.
  useEffect(() => {
    const handler = () => {
      if (!layers.cameras?.visible) toggleLayer('cameras');
    };
    window.addEventListener('japanosint:show-cameras-layer', handler);
    return () => window.removeEventListener('japanosint:show-cameras-layer', handler);
  }, [layers.cameras?.visible, toggleLayer]);

  const [popup, setPopup] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [isSearching, setIsSearching] = useState(false);
  const [searchError, setSearchError] = useState(null);
  const mapRef = useRef(null);

  const handleMapReady = useCallback((map) => {
    mapRef.current = map;
  }, []);

  const handleFeatureClick = useCallback((feature, layerType, lngLat) => {
    setPopup({ feature, layerType, lngLat });
  }, []);

  const popupPosition = useMapProjection(mapRef, popup?.lngLat);

  const handleClosePopup = useCallback(() => {
    setPopup(null);
  }, []);

  const handleSearch = useCallback(async (e) => {
    e.preventDefault();
    if (!searchQuery.trim()) return;

    setIsSearching(true);
    setSearchError(null);
    try {
      // Backend chains Nominatim -> Photon -> GSI, with caching.
      const res = await fetch(
        apiUrl(`/api/geocode?q=${encodeURIComponent(searchQuery)}`)
      );
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const { results } = await res.json();
      // Normalise to the shape the dropdown already expects.
      setSearchResults(
        (results || []).map((r) => ({
          display_name: r.display_name,
          lat: r.lat,
          lon: r.lon,
          source: r.source,
        }))
      );
    } catch (err) {
      // A failed lookup used to leave the PREVIOUS query's hits sitting in the
      // dropdown, under the new query — those places were then read as the
      // answer to a question they never answered.
      console.warn('[Search] Failed:', err.message);
      setSearchResults([]);
      setSearchError(err.message || 'geocode failed');
    } finally {
      setIsSearching(false);
    }
  }, [searchQuery]);

  // Layer collections carry the server's own `_meta` and the client's
  // `client_stored_at` (useMapLayers). The newest of those is the only honest
  // answer to "last update"; `new Date()` at render time simply printed the
  // clock and called it the data's age.
  const lastUpdate = React.useMemo(() => {
    let newest = null;
    for (const [id, st] of Object.entries(layers)) {
      if (!st.visible) continue;
      const t = layerData[id]?._meta?.client_stored_at;
      if (Number.isFinite(t) && (newest === null || t > newest)) newest = t;
    }
    return newest;
  }, [layers, layerData]);

  return (
    <div className="relative w-full h-full">
      {/* Map */}
      <MapView
        layers={layers}
        layerData={layerDataView}
        onFeatureClick={handleFeatureClick}
        onMapReady={handleMapReady}
      />

      {/* Layer Panel */}
      <LayerPanel
        layers={layers}
        layerData={layerData}
        onToggleLayer={toggleLayer}
        onSetOpacity={setLayerOpacity}
        onSetTemporalWindow={setLayerTemporalWindow}
        onSetAll={setAllLayers}
        cameraRunActive={!!cameraActiveRun}
      />

      {/* Search Box */}
      <div className="absolute top-3 left-1/2 -translate-x-1/2 z-30 w-80">
        <form onSubmit={handleSearch} className="relative">
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search location in Japan..."
            className="w-full px-4 py-2 bg-osint-surface/90 backdrop-blur-sm border border-osint-border rounded-lg text-sm text-gray-200 placeholder-gray-600 focus:outline-none focus:border-neon-cyan/40 focus:shadow-neon-cyan font-mono"
          />
          <button
            type="submit"
            className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-500 hover:text-neon-cyan text-sm"
            aria-label="Search"
          >
            {isSearching ? '...' : <MdSearch size={16} />}
          </button>
        </form>

        {searchError && (
          <div className="mt-1 glass-panel px-3 py-2 text-xs text-status-offline">
            Location search failed ({searchError}) — no results were obtained.
          </div>
        )}

        {/* Search results dropdown */}
        {searchResults.length > 0 && (
          <div className="mt-1 glass-panel overflow-hidden">
            {searchResults.map((r) => (
              <button
                key={`${r.lat},${r.lon}|${r.display_name}`}
                className="w-full text-left px-3 py-2 text-xs text-gray-300 hover:bg-neon-cyan/10 hover:text-neon-cyan border-b border-osint-border/50 last:border-0 transition-colors"
                onClick={() => {
                  setSearchResults([]);
                  setSearchError(null);
                  setSearchQuery(r.display_name.split(',')[0]);
                  // MapView listens for `japanosint:flyto` — same channel the
                  // Camera Discovery panel uses to recenter the map.
                  const lat = parseFloat(r.lat);
                  const lon = parseFloat(r.lon);
                  if (Number.isFinite(lat) && Number.isFinite(lon)) {
                    window.dispatchEvent(new CustomEvent('japanosint:flyto', {
                      detail: { lat, lon, zoom: 13 },
                    }));
                  }
                }}
              >
                <div className="truncate">{r.display_name}</div>
                <div className="text-[10px] font-mono text-gray-600 mt-0.5">
                  {parseFloat(r.lat).toFixed(4)}, {parseFloat(r.lon).toFixed(4)}
                </div>
              </button>
            ))}
          </div>
        )}
      </div>

      {/* Feature popup */}
      {popup && popupPosition && (
        <MapPopup
          feature={popup.feature}
          layerType={popup.layerType}
          position={popupPosition}
          onClose={handleClosePopup}
        />
      )}

      {/* Bottom info bar */}
      <div className="absolute bottom-0 left-0 right-0 z-20 flex items-center justify-between px-4 py-1.5 bg-osint-bg/85 backdrop-blur-sm border-t border-osint-border/50 text-[10px] font-mono text-gray-500">
        <div className="flex items-center gap-4">
          <span>Layers: <span className="text-neon-cyan">{activeCount}</span></span>
          <span>
            Features:{' '}
            <span className="text-neon-green">
              {Object.entries(layers)
                .filter(([, s]) => s.visible)
                .reduce((sum, [id]) => sum + (layerDataView[id]?.features?.length ?? 0), 0)}
            </span>
          </span>
        </div>
        <div>
          Last update:{' '}
          <span className="text-gray-400">
            {lastUpdate
              ? `${new Date(lastUpdate).toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })} JST`
              : 'no layer loaded'}
          </span>
        </div>
      </div>
    </div>
  );
}
