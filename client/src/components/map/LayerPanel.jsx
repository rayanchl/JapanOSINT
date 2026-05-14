import React, { useState } from 'react';
import { LAYER_DEFINITIONS, LAYER_CATEGORIES } from '../../hooks/useMapLayers';
import { getLayerIcon } from '../../utils/layerIcons';
import LoadingSpinner from '../ui/LoadingSpinner';

function LayerToggleItem({ id, def, state, onToggle, onOpacityChange, onTemporalChange, featureData, featureCount, forceLoading = false }) {
  const [showOpacity, setShowOpacity] = useState(false);
  const isActive = state.visible;
  const Icon = getLayerIcon(id);
  const showSpinner = state.loading || (forceLoading && isActive);

  // For temporal layers, derive the sorted list of distinct year_month
  // values present in the loaded data so the slider can snap to real months.
  const temporalKey = def.temporal ? (def.temporalKey || 'year_month') : null;
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
          <span className={`text-xs truncate ${isActive ? 'text-gray-200' : 'text-gray-500'}`}>
            {def.name}
          </span>
        </button>

        {/* Loading / Count */}
        <div className="flex-shrink-0 w-10 text-right">
          {showSpinner ? (
            <LoadingSpinner size="sm" />
          ) : featureCount > 0 ? (
            <span className="text-[10px] font-mono text-gray-500">{featureCount}</span>
          ) : null}
        </div>
      </div>

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
        </div>
      )}
    </div>
  );
}

export default function LayerPanel({
  layers,
  layerData,
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

  const layersByCategory = {};
  for (const cat of LAYER_CATEGORIES) {
    layersByCategory[cat] = [];
  }
  for (const [id, def] of Object.entries(LAYER_DEFINITIONS)) {
    if (def.hidden) continue; // upstream sources fused into a unified layer
    if (layersByCategory[def.category]) {
      layersByCategory[def.category].push(id);
    }
  }

  const getFeatureCount = (id) => {
    const data = layerData[id];
    return data?.features?.length ?? 0;
  };

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
            {LAYER_CATEGORIES.map((category) => {
              const ids = layersByCategory[category];
              if (!ids || ids.length === 0) return null;
              const isCollapsed = collapsedCategories.has(category);
              const activeInCat = ids.filter((id) => layers[id]?.visible).length;
              return (
                <div key={category} className="border-b border-osint-border/50">
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
                      {category}
                    </span>
                    <span className="font-mono text-gray-600">
                      {activeInCat > 0 ? `${activeInCat}/${ids.length}` : ids.length}
                    </span>
                  </button>
                  {!isCollapsed && ids.map((id) => (
                    <LayerToggleItem
                      key={id}
                      id={id}
                      def={LAYER_DEFINITIONS[id]}
                      state={layers[id]}
                      onToggle={onToggleLayer}
                      onOpacityChange={onSetOpacity}
                      onTemporalChange={onSetTemporalWindow}
                      featureData={layerData[id]}
                      featureCount={getFeatureCount(id)}
                      forceLoading={id === 'cameras' && cameraRunActive}
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
