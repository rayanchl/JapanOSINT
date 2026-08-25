import React, { useState } from 'react';
import { getLayerIcon } from '../../utils/layerIcons';
import LoadingSpinner from '../ui/LoadingSpinner';
import {
  groupByCategory,
  categoryLabel,
  classifyLayerData,
  truncationNotice,
} from '../../hooks/layerCatalog.js';

// Modality badge: the server's declaration, verbatim. `null` is shown as
// "geom" (rendered by each feature's own geometry) — never as a guess.
const MODALITY_BADGE = {
  point: { label: 'pt', title: 'modality: point' },
  heatmap: { label: 'heat', title: 'modality: heatmap' },
  line: { label: 'line', title: 'modality: line' },
  polygon: { label: 'poly', title: 'modality: polygon' },
  raster: { label: 'img', title: 'modality: raster' },
};

export function ModalityBadge({ modality }) {
  const b = MODALITY_BADGE[modality] || { label: 'geom', title: 'modality: undeclared by server — rendered by feature geometry' };
  return (
    <span
      className="text-[9px] font-mono px-1 rounded border border-osint-border-bright text-gray-500"
      title={b.title}
      data-testid="modality-badge"
    >
      {b.label}
    </span>
  );
}

/**
 * Status cell: spinner / 404 / err / count. A 404 ("the server has no such
 * layer"), an error ("we do not know") and an empty answer ("the server
 * holds zero records") are three different facts and get three different
 * cells.
 */
export function LayerStatusCell({ featureData, isActive, showSpinner }) {
  if (showSpinner) return <LoadingSpinner size="sm" />;
  const c = classifyLayerData(featureData);
  if (c.state === 'not_found') {
    return (
      <span className="text-[10px] font-mono text-status-offline" title="The server has no layer with this id (HTTP 404)" data-testid="status-404">
        404
      </span>
    );
  }
  if (c.state === 'error') {
    return (
      <span className="text-[10px] font-mono text-status-offline" title={`Load failed: ${c.message}`} data-testid="status-error">
        err
      </span>
    );
  }
  if (!isActive) return null;
  if (c.state === 'empty') {
    return (
      <span className="text-[10px] font-mono text-gray-500" title="The server answered and holds zero records for this layer" data-testid="status-empty">
        0
      </span>
    );
  }
  if (c.state === 'loaded') {
    return (
      <span className="text-[10px] font-mono text-gray-500" title={c.truncated ? `${c.count} of ${c.available} loaded` : `${c.count} records`} data-testid="status-count">
        {c.count}
      </span>
    );
  }
  return null;
}

function LayerToggleItem({ id, def, state, onToggle, onOpacityChange, onTemporalChange, featureData, forceLoading = false, renderNotices }) {
  const [showOpacity, setShowOpacity] = useState(false);
  const isActive = state.visible;
  const Icon = getLayerIcon(id);
  const showSpinner = state.loading || (forceLoading && isActive);

  // For temporal layers, derive the sorted list of distinct year_month
  // values present in the loaded data so the slider can snap to real months.
  const temporalKey = def.temporalKey || null;
  const months = (() => {
    if (!temporalKey) return null;
    const features = featureData?.features || [];
    if (features.length === 0) return [];
    const set = new Set();
    for (const f of features) {
      const v = f?.properties?.[temporalKey];
      if (v) set.add(String(v));
    }
    return Array.from(set).sort();
  })();
  const window = state.temporalWindow || null;
  const truncation = isActive ? truncationNotice(featureData) : null;
  const stillLoading = !!featureData?._meta?.client_loading;

  return (
    <div className={`layer-toggle px-3 py-2 ${isActive ? 'active' : ''}`}>
      <div className="flex items-center gap-2">
        {/* Toggle switch */}
        <button
          className={`toggle-switch flex-shrink-0 ${isActive ? 'on' : ''}`}
          onClick={() => onToggle(id)}
          aria-label={`Toggle ${def.name}`}
        />

        {/* Color dot */}
        <span
          className="w-2.5 h-2.5 rounded-full flex-shrink-0"
          style={{ background: def.color, boxShadow: isActive ? `0 0 6px ${def.color}66` : 'none' }}
        />

        {/* Icon + Name */}
        <button
          className="flex items-center gap-1.5 flex-1 min-w-0 text-left"
          onClick={() => setShowOpacity(!showOpacity)}
        >
          <Icon size={14} color={def.color} aria-hidden="true" />
          <span className="flex flex-col min-w-0">
            <span className={`text-xs truncate ${isActive ? 'text-gray-200' : 'text-gray-500'}`}>
              {def.name}
            </span>
            {/* Server taxonomy: modality badge + data_type. `data_type` null
              * is undeclared and is simply not shown. */}
            <span className="flex items-center gap-1 min-w-0">
              <ModalityBadge modality={def.modality ?? null} />
              {def.data_type && (
                <span className="text-[9px] font-mono truncate text-gray-600" title={`data_type: ${def.data_type}`}>
                  {def.data_type}
                </span>
              )}
              {def.clientOnly && (
                <span className="text-[9px] font-mono text-gray-600" title="Not in the server's layer taxonomy; fetched from /api/data">
                  local
                </span>
              )}
            </span>
            {/* A layer whose data is not purely observed says so here, next to
              * the switch that turns it on. Without this the transit layers
              * read as "these are the trains", when some markers are positions
              * this browser computed rather than positions anyone observed. */}
            {def.subtitle && (
              <span className="text-[10px] truncate text-amber-400/70" title={def.subtitle}>
                {def.subtitle}
              </span>
            )}
          </span>
        </button>

        <div className="flex-shrink-0 w-10 text-right">
          <LayerStatusCell featureData={featureData} isActive={isActive} showSpinner={showSpinner} />
        </div>
      </div>

      {/* In-band statement of a bounded view (house rule 2): the map is
        * drawing N of the M records the server holds. */}
      {truncation && (
        <div className="ml-10 mt-1 text-[10px] text-amber-400/80" data-testid="truncation-notice">
          {truncation}{stillLoading ? ' — loading the rest…' : ''}
          {featureData?._meta?.truncation_reason ? ` (${featureData._meta.truncation_reason})` : ''}
        </div>
      )}

      {/* Renderer notices (e.g. a raster layer whose records carry no image URL) */}
      {isActive && Array.isArray(renderNotices) && renderNotices.map((n) => (
        <div key={n.code} className="ml-10 mt-1 text-[10px] text-amber-400/80" data-testid={`render-notice-${n.code}`}>
          {n.message}
        </div>
      ))}

      {/* Opacity slider */}
      {showOpacity && isActive && (
        <div className="mt-2 ml-10 flex items-center gap-2">
          <span className="text-[10px] text-gray-500 w-8">Opacity</span>
          <input
            type="range"
            min="0"
            max="1"
            step="0.05"
            value={state.opacity}
            onChange={(e) => onOpacityChange(id, parseFloat(e.target.value))}
            className="flex-1 h-1 accent-neon-cyan bg-gray-700 rounded appearance-none cursor-pointer"
          />
          <span className="text-[10px] font-mono text-gray-500 w-8 text-right">
            {Math.round(state.opacity * 100)}%
          </span>
        </div>
      )}

      {/* Sources behind this layer, as the server declares them */}
      {showOpacity && isActive && def.sources && def.sources.length > 0 && (
        <div className="mt-1 ml-10 text-[10px] text-gray-500">
          {def.sources.length} source{def.sources.length === 1 ? '' : 's'}
          {def.kind ? ` · ${def.kind}` : ''}
          {Number.isFinite(def.records_geocoded) ? ` · ${def.records_geocoded.toLocaleString()} geocoded` : ''}
        </div>
      )}

      {/* Temporal window selector */}
      {showOpacity && isActive && temporalKey && months && months.length > 0 && (
        <div className="mt-2 ml-10">
          <div className="flex items-center gap-2 mb-1">
            <span className="text-[10px] text-gray-500 w-8">Window</span>
            <select
              value={window ? `${window[0]}|${window[1]}` : 'all'}
              onChange={(e) => {
                const v = e.target.value;
                if (v === 'all') onTemporalChange?.(id, null);
                else {
                  const [s, e2] = v.split('|');
                  onTemporalChange?.(id, [s, e2]);
                }
              }}
              className="flex-1 text-[10px] bg-osint-surface border border-osint-border-bright rounded px-1 py-0.5 text-gray-300"
            >
              <option value="all">All ({months.length})</option>
              {months.map((m) => (
                <option key={m} value={`${m}|${m}`}>{m}</option>
              ))}
              {months.length > 1 && (
                <option value={`${months[0]}|${months[months.length - 1]}`}>
                  Range {months[0]} → {months[months.length - 1]}
                </option>
              )}
            </select>
          </div>
          {/* State the bound: the map is drawing the window, not the layer. */}
          {window && (
            <div className="text-[10px] text-amber-400/80">
              {(() => {
                const total = featureData?.features?.length ?? 0;
                const shown = (featureData?.features || []).filter((f) => {
                  const v = f?.properties?.[temporalKey];
                  if (v == null) return false;
                  const sv = String(v);
                  return sv >= String(window[0]) && sv <= String(window[1]);
                }).length;
                return `Showing ${shown.toLocaleString()} of ${total.toLocaleString()} features in this window.`;
              })()}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default function LayerPanel({
  layers,
  layerData,
  catalog = {},
  catalogStatus = 'ready',
  catalogError = null,
  renderNotices = {},
  onToggleLayer,
  onSetOpacity,
  onSetTemporalWindow,
  onSetAll,
  cameraRunActive = false,
}) {
  const [collapsed, setCollapsed] = useState(false);
  const [collapsedCategories, setCollapsedCategories] = useState(() => new Set());

  const toggleCategory = (category) => {
    setCollapsedCategories((prev) => {
      const next = new Set(prev);
      if (next.has(category)) next.delete(category);
      else next.add(category);
      return next;
    });
  };

  // Grouped by the SERVER's category. A category this client has never
  // heard of is appended, not dropped.
  const groups = groupByCategory(catalog).map(([cat, ids]) => [cat, ids.filter((id) => layers[id])]);

  const activeCount = Object.values(layers).filter((l) => l.visible).length;

  return (
    <div
      className={`layer-panel absolute top-0 left-0 h-full z-30 transition-all duration-300 flex ${
        collapsed ? 'w-10' : 'w-64'
      }`}
    >
      {/* Collapse toggle */}
      <div className="flex flex-col">
        {!collapsed && (
          <div className="flex-1 w-64 overflow-y-auto">
            {/* Header */}
            <div className="px-3 py-3 border-b border-osint-border">
              <div className="flex items-center justify-between">
                <h2 className="text-xs font-semibold uppercase tracking-wider text-neon-cyan">
                  Layers
                </h2>
                <span className="text-[10px] font-mono text-gray-500">{activeCount} active</span>
              </div>

              {/* Where the taxonomy came from. The static table is not the
                * server's catalogue and must not be presented as it. */}
              {catalogStatus === 'loading' && (
                <div className="mt-1 text-[10px] text-gray-500" data-testid="catalog-status">
                  Loading layer catalogue from server…
                </div>
              )}
              {catalogStatus === 'error' && (
                <div className="mt-1 text-[10px] text-status-offline" data-testid="catalog-status">
                  Server layer catalogue not obtained ({catalogError || 'request failed'}); showing the client table only.
                </div>
              )}

              <div className="flex gap-2 mt-2">
                <button
                  onClick={() => onSetAll(true)}
                  className="text-[10px] px-2 py-0.5 rounded border border-osint-border-bright text-gray-400 hover:text-neon-cyan hover:border-neon-cyan/30 transition-colors"
                >
                  All On
                </button>
                <button
                  onClick={() => onSetAll(false)}
                  className="text-[10px] px-2 py-0.5 rounded border border-osint-border-bright text-gray-400 hover:text-neon-red hover:border-neon-red/30 transition-colors"
                >
                  All Off
                </button>
              </div>
            </div>

            {/* Layer groups */}
            {groups.map(([category, ids]) => {
              if (!ids || ids.length === 0) return null;
              const isCollapsed = collapsedCategories.has(category);
              const activeInCat = ids.filter((id) => layers[id]?.visible).length;
              return (
                <div key={category} className="border-b border-osint-border/50" data-testid={`category-${category}`}>
                  <button
                    type="button"
                    onClick={() => toggleCategory(category)}
                    className="w-full flex items-center justify-between px-3 py-1.5 text-[10px] uppercase tracking-widest text-gray-500 font-medium hover:bg-osint-surface/40 hover:text-gray-300 transition-colors"
                    aria-expanded={!isCollapsed}
                  >
                    <span className="flex items-center gap-1.5">
                      <span className="inline-block w-3 text-gray-600">
                        {isCollapsed ? '▸' : '▾'}
                      </span>
                      {categoryLabel(category)}
                    </span>
                    <span className="font-mono text-gray-600">
                      {activeInCat > 0 ? `${activeInCat}/${ids.length}` : ids.length}
                    </span>
                  </button>
                  {!isCollapsed && ids.map((id) => (
                    <LayerToggleItem
                      key={id}
                      id={id}
                      def={catalog[id]}
                      state={layers[id]}
                      onToggle={onToggleLayer}
                      onOpacityChange={onSetOpacity}
                      onTemporalChange={onSetTemporalWindow}
                      featureData={layerData[id]}
                      forceLoading={id === 'cameras' && cameraRunActive}
                      renderNotices={renderNotices[id]}
                    />
                  ))}
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Toggle button */}
      <button
        onClick={() => setCollapsed(!collapsed)}
        className="absolute top-3 bg-osint-surface border border-osint-border rounded-r px-1 py-2 text-gray-400 hover:text-neon-cyan transition-colors z-40"
        style={{ left: collapsed ? 0 : '256px' }}
        aria-label={collapsed ? 'Expand layer panel' : 'Collapse layer panel'}
      >
        <span className="text-xs">{collapsed ? '>>' : '<<'}</span>
      </button>
    </div>
  );
}
