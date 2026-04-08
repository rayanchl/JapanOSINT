import React, { useState } from 'react';
import { LAYER_DEFINITIONS, LAYER_CATEGORIES } from '../../hooks/useMapLayers';
import LoadingSpinner from '../ui/LoadingSpinner';

function LayerToggleItem({ id, def, state, onToggle, onOpacityChange, featureCount }) {
  const [showOpacity, setShowOpacity] = useState(false);
  const isActive = state.visible;

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
          <span className="text-sm">{def.icon}</span>
          <span className={`text-xs truncate ${isActive ? 'text-gray-200' : 'text-gray-500'}`}>
            {def.name}
          </span>
        </button>

        {/* Loading / Count */}
        <div className="flex-shrink-0 w-10 text-right">
          {state.loading ? (
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
    </div>
  );
}

export default function LayerPanel({
  layers,
  layerData,
  onToggleLayer,
  onSetOpacity,
  onSetAll,
}) {
  const [collapsed, setCollapsed] = useState(false);

  const layersByCategory = {};
  for (const cat of LAYER_CATEGORIES) {
    layersByCategory[cat] = [];
  }
  for (const [id, def] of Object.entries(LAYER_DEFINITIONS)) {
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
              return (
                <div key={category} className="border-b border-osint-border/50">
                  <div className="px-3 py-1.5 text-[10px] uppercase tracking-widest text-gray-600 font-medium">
                    {category}
                  </div>
                  {ids.map((id) => (
                    <LayerToggleItem
                      key={id}
                      id={id}
                      def={LAYER_DEFINITIONS[id]}
                      state={layers[id]}
                      onToggle={onToggleLayer}
                      onOpacityChange={onSetOpacity}
                      featureCount={getFeatureCount(id)}
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
