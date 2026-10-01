import React, { useState, useCallback, useEffect, useRef, useMemo } from 'react';
import { useSearchParams } from 'react-router-dom';
import { LuX } from 'react-icons/lu';
import MapView from './MapView';
import LayerPanel from './LayerPanel';
import MapPopup from './MapPopup';
import MapTopBar from './overlays/MapTopBar.jsx';
import AOIDrawOverlay, { AoiLayer } from './overlays/AOIDrawOverlay.jsx';
import IsochroneOverlay from './overlays/IsochroneOverlay.jsx';
import TimeSlider from './overlays/TimeSlider.jsx';
import ShareSheet from './overlays/ShareSheet.jsx';
import FeatureStats from './overlays/FeatureStats.jsx';
import useMapLayers from '../../hooks/useMapLayers';
import useMapProjection from '../../hooks/useMapProjection';
import useCameraDiscoveryStream from '../../hooks/useCameraDiscoveryStream';
import useTimeWindow, { applyTimeWindow } from '../../hooks/useTimeWindow.js';
import useAoi from '../../hooks/useAoi.js';
import usePermalink from '../../hooks/usePermalink.js';
import apiUrl from '../../utils/apiUrl.js';
import { api } from '../../api/client.js';
import { CopyButton } from '../ui/kit.jsx';

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
    catalog,
    catalogStatus,
    catalogError,
  } = useMapLayers();

  // Renderer notices per layer (e.g. a raster layer whose records carry no
  // image URL): surfaced in the panel, in band, next to the toggle.
  const [renderNotices, setRenderNotices] = useState({});
  const handleRenderNotice = useCallback((layerId, notices) => {
    setRenderNotices((prev) => {
      const had = prev[layerId];
      if ((!notices || notices.length === 0) && !had) return prev;
      if (had && notices && had.length === notices.length && had.every((n, i) => n.code === notices[i].code && n.message === notices[i].message)) return prev;
      const next = { ...prev };
      if (!notices || notices.length === 0) delete next[layerId];
      else next[layerId] = notices;
      return next;
    });
  }, []);

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
  const mapRef = useRef(null);
  // A state counter so overlays that subscribe to the map instance re-run
  // their effects once the map exists (a ref alone would not re-render).
  const [mapReadyTick, setMapReadyTick] = useState(0);

  // ── iOS Map-tab parity: time window, AOI drawing, isochrone, share, stats ──
  const [searchParams, setSearchParams] = useSearchParams();
  const tw = useTimeWindow();
  const [timeOpen, setTimeOpen] = useState(false);
  const [timeCollapsed, setTimeCollapsed] = useState(true);
  const [aoiDrawing, setAoiDrawing] = useState(false);
  const [aoiLayerOn, setAoiLayerOn] = useState(false);
  const aoi = useAoi({ enabled: aoiLayerOn });
  const [isoOpen, setIsoOpen] = useState(false);
  const [shareOpen, setShareOpen] = useState(false);
  const [statsOpen, setStatsOpen] = useState(false);
  const [reverse, setReverse] = useState(null); // { lat, lon, label?, source?, error?, busy }
  const { resolve: resolvePermalink } = usePermalink();
  const [permalinkError, setPermalinkError] = useState(null);
  const aoiDrawingRef = useRef(false);
  aoiDrawingRef.current = aoiDrawing;

  const handleMapReady = useCallback((map) => {
    mapRef.current = map;
    setMapReadyTick((t) => t + 1);
  }, []);

  const handleFeatureClick = useCallback((feature, layerType, lngLat) => {
    // While drawing an AOI, map clicks place vertices — not popups.
    if (aoiDrawingRef.current) return;
    setPopup({ feature, layerType, lngLat });
  }, []);

  const popupPosition = useMapProjection(mapRef, popup?.lngLat);
  const reversePosition = useMapProjection(mapRef, reverse ? [reverse.lon, reverse.lat] : null);

  const handleClosePopup = useCallback(() => {
    setPopup(null);
  }, []);

  // Reverse geocode a point (right-click on the map, or the top-bar centre
  // button). A failed lookup is shown as failed, not as an empty label.
  const reverseGeocodeAt = useCallback(async (lat, lon) => {
    setReverse({ lat, lon, busy: true });
    try {
      const j = await api.get('/api/geocode/reverse', { query: { lat, lon } });
      setReverse({ lat, lon, busy: false, label: j?.display_name || null, source: j?.source || null, empty: !j?.display_name });
    } catch (e) {
      setReverse({ lat, lon, busy: false, error: e });
    }
  }, []);

  const reverseGeocodeCentre = useCallback(() => {
    const c = mapRef.current?.getCenter?.();
    if (c) reverseGeocodeAt(c.lat, c.lng);
  }, [reverseGeocodeAt]);

  useEffect(() => {
    const map = mapRef.current;
    if (!map) return undefined;
    const onCtx = (e) => { e.preventDefault?.(); reverseGeocodeAt(e.lngLat.lat, e.lngLat.lng); };
    map.on('contextmenu', onCtx);
    return () => map.off('contextmenu', onCtx);
  }, [mapReadyTick, reverseGeocodeAt]);

  // `?aoi=new` (from Console → Areas of interest) starts drawing.
  useEffect(() => {
    if (searchParams.get('aoi') === 'new') {
      setAoiDrawing(true);
      const next = new URLSearchParams(searchParams);
      next.delete('aoi');
      setSearchParams(next, { replace: true });
    }
  }, [searchParams, setSearchParams]);

  // `?p=<token>` restores a shared view: camera, layers, time window.
  const permalinkApplied = useRef(false);
  useEffect(() => {
    const token = searchParams.get('p');
    if (!token || permalinkApplied.current || !mapRef.current) return;
    permalinkApplied.current = true;
    (async () => {
      try {
        const st = await resolvePermalink(token);
        if (!st) return;
        if (Array.isArray(st.layers) && st.layers.length) {
          const wanted = new Set(st.layers);
          for (const id of Object.keys(layers)) {
            const want = wanted.has(id);
            if (want !== !!layers[id]?.visible) toggleLayer(id);
          }
        }
        if (st.map && Number.isFinite(st.map.lat) && Number.isFinite(st.map.lon)) {
          mapRef.current.jumpTo({ center: [st.map.lon, st.map.lat], zoom: st.map.zoom ?? 10, bearing: st.map.bearing ?? 0, pitch: st.map.pitch ?? 0 });
        }
        if (st.params?.at) {
          const at = Date.parse(st.params.at);
          if (Number.isFinite(at)) { tw.setAt(at); if (st.params.window) tw.setWindowSec(Number(st.params.window)); setTimeOpen(true); }
        }
      } catch (e) {
        // A link that cannot be resolved is shown as such — not as the default view.
        setPermalinkError(e);
      } finally {
        const next = new URLSearchParams(searchParams);
        next.delete('p');
        setSearchParams(next, { replace: true });
      }
    })();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [searchParams, mapReadyTick]);

  // The global window filters every visible time-coded layer; the report
  // says which visible layers it could NOT be applied to.
  const timed = useMemo(
    () => applyTimeWindow(layerDataView, layers, catalog, { at: tw.at, windowSec: tw.windowSec }),
    [layerDataView, layers, catalog, tw.at, tw.windowSec],
  );

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
        layerData={timed.view}
        catalog={catalog}
        onFeatureClick={handleFeatureClick}
        onMapReady={handleMapReady}
        onRenderNotice={handleRenderNotice}
      />

      {/* Layer Panel */}
      <LayerPanel
        layers={layers}
        layerData={layerData}
        catalog={catalog}
        catalogStatus={catalogStatus}
        catalogError={catalogError}
        renderNotices={renderNotices}
        onToggleLayer={toggleLayer}
        onSetOpacity={setLayerOpacity}
        onSetTemporalWindow={setLayerTemporalWindow}
        onSetAll={setAllLayers}
        cameraRunActive={!!cameraActiveRun}
      />

      {/* Floating top bar: layers pill · geocode · reverse-geocode · more menu */}
      <MapTopBar
        mapRef={mapRef}
        layers={layers}
        layerDataView={timed.view}
        catalog={catalog}
        activeCount={activeCount}
        onOpenIsochrone={() => { setIsoOpen(true); setStatsOpen(false); }}
        onOpenShare={() => setShareOpen(true)}
        onToggleStats={() => { setStatsOpen((v) => !v); setIsoOpen(false); }}
        statsOpen={statsOpen}
        onStartAoi={() => { setAoiDrawing(true); setPopup(null); }}
        aoiLayerOn={aoiLayerOn}
        onToggleAoiLayer={() => setAoiLayerOn((v) => !v)}
        timeOpen={timeOpen}
        onToggleTime={() => { setTimeOpen((v) => !v); setTimeCollapsed(false); }}
        onReverseGeocode={reverseGeocodeCentre}
      />

      {/* Saved areas of interest (optional layer) + drawing */}
      <AoiLayer mapRef={mapRef} geojson={aoi.geojson} visible={aoiLayerOn && mapReadyTick > 0} />
      {aoiLayerOn && aoi.error && (
        <div className="absolute top-16 right-3 z-30 glass-panel px-3 py-2 text-xs text-neon-red max-w-xs">
          Areas of interest could not be loaded ({aoi.error.message}). Nothing is drawn — this is not a statement that none exist.
        </div>
      )}
      {aoiLayerOn && aoi.truncated && (
        <div className="absolute top-16 right-3 z-30 glass-panel px-3 py-2 text-xs text-accent max-w-xs">Showing the first {aoi.rows.length} areas — the list has more pages.</div>
      )}
      <AOIDrawOverlay mapRef={mapRef} active={aoiDrawing && mapReadyTick > 0} onClose={() => setAoiDrawing(false)} onSave={aoi.create} />

      {/* Travel-time reachability */}
      <IsochroneOverlay mapRef={mapRef} open={isoOpen && mapReadyTick > 0} onClose={() => setIsoOpen(false)} />

      {/* Feature stats */}
      <FeatureStats mapRef={mapRef} layers={layers} view={timed.view} catalog={catalog} open={statsOpen} onClose={() => setStatsOpen(false)} />

      {/* Share permalink */}
      <ShareSheet open={shareOpen} onClose={() => setShareOpen(false)} mapRef={mapRef} layers={layers} timeWindow={tw} />

      {/* Time window + playback */}
      {timeOpen && (
        <TimeSlider tw={tw} report={timed.report} view={timed.view} layers={layers} catalog={catalog} collapsed={timeCollapsed} onToggleCollapsed={() => setTimeCollapsed((v) => !v)} />
      )}

      {permalinkError && (
        <div className="absolute top-16 left-1/2 -translate-x-1/2 z-40 glass-panel px-3 py-2 text-xs text-neon-red max-w-md flex items-start gap-2">
          <span>Shared view could not be restored ({permalinkError.message}). The map is showing its default view, not the shared one.</span>
          <button type="button" onClick={() => setPermalinkError(null)} className="text-osint-muted hover:text-osint-text" aria-label="Dismiss"><LuX size={13} /></button>
        </div>
      )}

      {/* Reverse-geocode card (right-click / centre probe) */}
      {reverse && reversePosition && (
        <div className="absolute z-40 glass-panel map-popup-ridge p-3 min-w-[220px] max-w-[320px] shadow-lg" style={{ left: reversePosition.x, top: reversePosition.y, transform: 'translate(-50%, -110%)' }}>
          <div className="flex items-center justify-between mb-1">
            <span className="font-mono text-[10px] uppercase tracking-wider text-accent">Reverse geocode</span>
            <button type="button" onClick={() => setReverse(null)} className="text-osint-muted hover:text-osint-text" aria-label="Close"><LuX size={13} /></button>
          </div>
          {reverse.busy && <div className="text-xs text-osint-muted">Looking up…</div>}
          {reverse.error && <div className="text-xs text-neon-red">Lookup failed ({reverse.error.message}) — no address was obtained.</div>}
          {!reverse.busy && !reverse.error && (reverse.label
            ? <div className="text-sm text-osint-text">{reverse.label}{reverse.source && <span className="ml-1 text-[10px] font-mono text-osint-muted">via {reverse.source}</span>}</div>
            : <div className="text-xs text-osint-muted">The geocoder returned no address for this point.</div>)}
          <div className="mt-1 flex items-center gap-2 font-mono text-[10px] text-osint-muted">
            {reverse.lat.toFixed(5)}, {reverse.lon.toFixed(5)}
            <CopyButton text={`${reverse.lat.toFixed(6)}, ${reverse.lon.toFixed(6)}`} />
          </div>
        </div>
      )}

      {/* Feature popup */}
      {popup && popupPosition && !aoiDrawing && (
        <MapPopup
          feature={popup.feature}
          layerType={popup.layerType}
          layerDef={catalog[popup.layerType]}
          position={popupPosition}
          onClose={handleClosePopup}
        />
      )}

      {/* Bottom info bar */}
      <div className="absolute bottom-0 left-0 right-0 z-20 flex items-center justify-between px-4 py-1.5 bg-osint-bg/85 backdrop-blur-sm border-t border-osint-border/50 text-[10px] font-mono text-osint-muted">
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
          <span className="text-osint-muted">
            {lastUpdate
              ? `${new Date(lastUpdate).toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })} JST`
              : 'no layer loaded'}
          </span>
        </div>
      </div>
    </div>
  );
}
