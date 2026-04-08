import React from 'react';

function EarthquakeDetail({ properties }) {
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-2xl font-mono font-bold text-neon-red">
          M{properties.magnitude ?? properties.mag ?? '?'}
        </span>
        {properties.depth != null && (
          <span className="text-xs text-gray-400">Depth: {properties.depth} km</span>
        )}
      </div>
      {properties.place && <p className="text-sm text-gray-300">{properties.place}</p>}
      {properties.time && (
        <p className="text-xs text-gray-500 font-mono">
          {new Date(properties.time).toLocaleString('en-GB', { timeZone: 'Asia/Tokyo' })} JST
        </p>
      )}
      {properties.intensity && (
        <div className="text-xs">
          <span className="text-gray-400">Intensity: </span>
          <span className="text-neon-orange font-mono">{properties.intensity}</span>
        </div>
      )}
    </div>
  );
}

function CameraDetail({ properties }) {
  return (
    <div className="space-y-2">
      <p className="text-sm font-medium text-gray-200">{properties.name || 'Camera'}</p>
      {properties.thumbnail && (
        <img
          src={properties.thumbnail}
          alt={properties.name || 'Camera feed'}
          className="w-full rounded border border-osint-border"
          loading="lazy"
        />
      )}
      {properties.location && (
        <p className="text-xs text-gray-400">{properties.location}</p>
      )}
      {properties.url && (
        <a
          href={properties.url}
          target="_blank"
          rel="noopener noreferrer"
          className="text-xs text-neon-cyan hover:underline"
        >
          View Feed
        </a>
      )}
    </div>
  );
}

function WeatherDetail({ properties }) {
  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{properties.station || properties.name || 'Station'}</p>
      {properties.temperature != null && (
        <p className="font-mono text-neon-cyan text-lg">{properties.temperature}°C</p>
      )}
      {properties.humidity != null && (
        <p className="text-xs text-gray-400">Humidity: {properties.humidity}%</p>
      )}
      {properties.wind_speed != null && (
        <p className="text-xs text-gray-400">Wind: {properties.wind_speed} m/s</p>
      )}
      {properties.condition && (
        <p className="text-xs text-gray-300">{properties.condition}</p>
      )}
    </div>
  );
}

function AirQualityDetail({ properties }) {
  const aqi = properties.aqi ?? properties.value;
  let color = '#00ff88';
  let label = 'Good';
  if (aqi > 150) { color = '#ff4444'; label = 'Unhealthy'; }
  else if (aqi > 100) { color = '#ff8c00'; label = 'Moderate-High'; }
  else if (aqi > 50) { color = '#ffb74d'; label = 'Moderate'; }

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{properties.station || properties.name || 'Station'}</p>
      <div className="flex items-center gap-2">
        <span className="text-2xl font-mono font-bold" style={{ color }}>{aqi ?? '?'}</span>
        <span className="text-xs px-2 py-0.5 rounded" style={{ background: color + '22', color }}>{label}</span>
      </div>
    </div>
  );
}

function RadiationDetail({ properties }) {
  const value = properties.value ?? properties.nGy;
  let color = '#00ff88';
  if (value > 100) color = '#ff4444';
  else if (value > 50) color = '#ffd600';

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{properties.station || properties.name || 'Station'}</p>
      <p className="font-mono text-xl font-bold" style={{ color }}>
        {value ?? '?'} <span className="text-xs text-gray-400">nGy/h</span>
      </p>
    </div>
  );
}

function GenericDetail({ properties, layerType }) {
  const skip = new Set(['id', 'layerType', 'geometry', 'coordinates', '_index']);
  const entries = Object.entries(properties).filter(([k]) => !skip.has(k));

  return (
    <div className="space-y-1">
      {entries.slice(0, 8).map(([key, value]) => (
        <div key={key} className="flex justify-between text-xs gap-3">
          <span className="text-gray-400 capitalize">{key.replace(/_/g, ' ')}</span>
          <span className="text-gray-200 font-mono text-right truncate max-w-[180px]">
            {typeof value === 'object' ? JSON.stringify(value) : String(value)}
          </span>
        </div>
      ))}
    </div>
  );
}

const DETAIL_RENDERERS = {
  earthquakes: EarthquakeDetail,
  cameras: CameraDetail,
  weather: WeatherDetail,
  airQuality: AirQualityDetail,
  radiation: RadiationDetail,
};

export default function MapPopup({ feature, layerType, onClose, position }) {
  if (!feature) return null;

  const properties = feature.properties || feature;
  const Renderer = DETAIL_RENDERERS[layerType] || GenericDetail;
  const layerLabel = layerType
    ? layerType.replace(/([A-Z])/g, ' $1').replace(/^./, (s) => s.toUpperCase())
    : 'Feature';

  return (
    <div
      className="absolute z-40 glass-panel p-3 min-w-[220px] max-w-[320px] shadow-lg"
      style={{
        left: position?.x ?? 0,
        top: position?.y ?? 0,
        transform: 'translate(-50%, -110%)',
      }}
    >
      <div className="flex items-center justify-between mb-2">
        <span className="text-[10px] uppercase tracking-wider text-neon-cyan font-medium">
          {layerLabel}
        </span>
        <button
          onClick={onClose}
          className="text-gray-500 hover:text-gray-200 text-sm leading-none ml-2"
          aria-label="Close popup"
        >
          x
        </button>
      </div>
      <Renderer properties={properties} layerType={layerType} />
    </div>
  );
}
