import React, { useState, useCallback } from 'react';
import MapView from './MapView';
import LayerPanel from './LayerPanel';
import MapPopup from './MapPopup';
import useMapLayers from '../../hooks/useMapLayers';

export default function MapPage() {
  const {
    layers,
    toggleLayer,
    setLayerOpacity,
    setAllLayers,
    layerData,
    activeCount,
  } = useMapLayers();

  const [popup, setPopup] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [isSearching, setIsSearching] = useState(false);

  const handleFeatureClick = useCallback((feature, layerType, position) => {
    setPopup({ feature, layerType, position });
  }, []);

  const handleClosePopup = useCallback(() => {
    setPopup(null);
  }, []);

  const handleSearch = useCallback(async (e) => {
    e.preventDefault();
    if (!searchQuery.trim()) return;

    setIsSearching(true);
    try {
      // Backend chains Nominatim -> Photon -> GSI, with caching.
      const res = await fetch(
        `/api/geocode?q=${encodeURIComponent(searchQuery)}`
      );
      if (res.ok) {
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
      }
    } catch (err) {
      console.warn('[Search] Failed:', err.message);
    } finally {
      setIsSearching(false);
    }
  }, [searchQuery]);

  const lastUpdate = new Date().toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' });

  return (
    <div className="relative w-full h-full">
      {/* Map */}
      <MapView
        layers={layers}
        layerData={layerData}
        onFeatureClick={handleFeatureClick}
      />

      {/* Layer Panel */}
      <LayerPanel
        layers={layers}
        layerData={layerData}
        onToggleLayer={toggleLayer}
        onSetOpacity={setLayerOpacity}
        onSetAll={setAllLayers}
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
            {isSearching ? '...' : '\u{1F50D}'}
          </button>
        </form>

        {/* Search results dropdown */}
        {searchResults.length > 0 && (
          <div className="mt-1 glass-panel overflow-hidden">
            {searchResults.map((r, i) => (
              <button
                key={i}
                className="w-full text-left px-3 py-2 text-xs text-gray-300 hover:bg-neon-cyan/10 hover:text-neon-cyan border-b border-osint-border/50 last:border-0 transition-colors"
                onClick={() => {
                  setSearchResults([]);
                  setSearchQuery(r.display_name.split(',')[0]);
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
      {popup && (
        <MapPopup
          feature={popup.feature}
          layerType={popup.layerType}
          position={popup.position}
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
                .reduce((sum, [id]) => sum + (layerData[id]?.features?.length ?? 0), 0)}
            </span>
          </span>
        </div>
        <div>
          Last update: <span className="text-gray-400">{lastUpdate} JST</span>
        </div>
      </div>
    </div>
  );
}
