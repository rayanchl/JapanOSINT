import React, { useEffect, useState } from 'react';

/**
 * Fetch a reverse-geocoded address label for a feature, using the server's
 * multi-provider chain (Nominatim -> Photon -> GSI). Renders nothing until
 * the lookup resolves, and silently renders nothing on failure.
 */
function ReverseGeocodeLabel({ feature }) {
  const [label, setLabel] = useState(null);

  useEffect(() => {
    let cancelled = false;
    const coords = feature?.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) return;
    const [lon, lat] = coords;
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return;

    // Skip if the feature already has a human address tag.
    const p = feature.properties || {};
    if (p.address || p['addr:full']) return;

    (async () => {
      try {
        const res = await fetch(`/api/geocode/reverse?lat=${lat}&lon=${lon}`);
        if (!res.ok) return;
        const data = await res.json();
        if (!cancelled && data?.display_name) {
          setLabel({ text: data.display_name, source: data.source });
        }
      } catch { /* silent */ }
    })();

    return () => { cancelled = true; };
  }, [feature]);

  if (!label) return null;
  return (
    <div className="mt-2 pt-2 border-t border-osint-border/50">
      <div className="text-[10px] uppercase tracking-wider text-gray-500 mb-0.5">
        Address <span className="text-gray-600">({label.source})</span>
      </div>
      <p className="text-xs text-gray-300 leading-snug">{label.text}</p>
    </div>
  );
}

/* ---------- shared value helpers ---------- */

const SKIP_KEYS = new Set([
  'id', 'layerType', 'geometry', 'coordinates', '_index', '_idx',
]);

// Keys that look like ISO-8601 timestamps or unix seconds/millis.
const TIME_KEY_RE = /(^|_)(time|timestamp|datetime|date|at|measured_at|updated_at|fetched_at|report_datetime|last_seen|forecast_date)$/i;

function isIsoDate(v) {
  if (typeof v !== 'string') return false;
  if (v.length < 8) return false;
  return /^\d{4}[-/]\d{1,2}[-/]\d{1,2}([T ]|$)/.test(v);
}

function formatTimestamp(v) {
  try {
    const d = new Date(v);
    if (Number.isNaN(d.getTime())) return String(v);
    return d.toLocaleString('en-GB', { timeZone: 'Asia/Tokyo' }) + ' JST';
  } catch {
    return String(v);
  }
}

function formatValue(key, value) {
  if (value == null || value === '') return '-';
  if (typeof value === 'boolean') return value ? 'Yes' : 'No';
  if (Array.isArray(value)) return value.join(', ');
  if (typeof value === 'object') return JSON.stringify(value);
  if (typeof value === 'string' && (TIME_KEY_RE.test(key) || isIsoDate(value))) {
    return formatTimestamp(value);
  }
  if (typeof value === 'number') {
    // Keep small ints and short decimals as-is; format huge counts.
    if (Math.abs(value) >= 10_000 && Number.isInteger(value)) {
      return value.toLocaleString('en-US');
    }
    return String(value);
  }
  return String(value);
}

function labelize(key) {
  return key.replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
}

function isUrl(v) {
  return typeof v === 'string' && /^https?:\/\//i.test(v);
}

/**
 * Render every remaining property (after a renderer has shown its highlighted
 * fields). Formats timestamps, booleans, URLs; shows all rows, scrollable.
 */
function PropertyTable({ properties, exclude = [] }) {
  const hide = new Set([...SKIP_KEYS, ...exclude]);
  const entries = Object.entries(properties).filter(([k, v]) => {
    if (hide.has(k)) return false;
    if (v == null || v === '') return false;
    return true;
  });
  if (entries.length === 0) return null;

  return (
    <div className="mt-2 pt-2 border-t border-osint-border/50 max-h-[220px] overflow-y-auto pr-1 space-y-0.5">
      {entries.map(([key, value]) => (
        <div key={key} className="flex justify-between items-baseline text-xs gap-3">
          <span className="text-gray-500 shrink-0">{labelize(key)}</span>
          <span className="text-gray-200 font-mono text-right break-all">
            {isUrl(value) ? (
              <a
                href={value}
                target="_blank"
                rel="noopener noreferrer"
                className="text-neon-cyan hover:underline"
              >
                link
              </a>
            ) : (
              formatValue(key, value)
            )}
          </span>
        </div>
      ))}
    </div>
  );
}

/* ---------- per-layer renderers ---------- */

function EarthquakeDetail({ properties }) {
  const mag = properties.magnitude ?? properties.mag;
  const depth = properties.depth_km ?? properties.depth;
  const intensity = properties.max_intensity ?? properties.intensity;
  const time = properties.timestamp ?? properties.time ?? properties.at;
  const highlighted = ['magnitude', 'mag', 'depth_km', 'depth', 'max_intensity', 'intensity', 'timestamp', 'time', 'at', 'place'];
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-2xl font-mono font-bold text-neon-red">
          M{mag ?? '?'}
        </span>
        {depth != null && (
          <span className="text-xs text-gray-400">Depth: {depth} km</span>
        )}
        {intensity != null && (
          <span className="text-xs px-1.5 py-0.5 rounded bg-neon-orange/20 text-neon-orange font-mono">
            震度 {intensity}
          </span>
        )}
      </div>
      {properties.place && <p className="text-sm text-gray-300">{properties.place}</p>}
      {time && (
        <p className="text-xs text-gray-500 font-mono">
          {formatTimestamp(time)}
        </p>
      )}
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function CameraDetail({ properties }) {
  const highlighted = ['name', 'thumbnail', 'location', 'url'];
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
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function WeatherDetail({ properties }) {
  const name = properties.prefecture_name || properties.station || properties.name || properties.target_area || 'Weather';
  const tempHigh = properties.temperature_high ?? properties.temp_high;
  const tempLow = properties.temperature_low ?? properties.temp_low;
  const temp = properties.temperature;
  const condition = properties.weather_condition || properties.condition;
  const overview = properties.weather_overview;
  const wind = properties.wind_speed_ms ?? properties.wind_speed;
  const windDir = properties.wind_direction;
  const humidity = properties.humidity_percent ?? properties.humidity;
  const precip = properties.precipitation_probability;

  const highlighted = [
    'prefecture_name', 'station', 'name', 'target_area',
    'temperature_high', 'temp_high', 'temperature_low', 'temp_low', 'temperature',
    'weather_condition', 'condition', 'weather_overview',
    'wind_speed_ms', 'wind_speed', 'wind_direction',
    'humidity_percent', 'humidity', 'precipitation_probability',
  ];

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{name}</p>
      {condition && <p className="text-sm text-gray-300">{condition}</p>}
      {temp != null && (
        <p className="font-mono text-neon-cyan text-lg">{temp}°C</p>
      )}
      {(tempHigh != null || tempLow != null) && (
        <p className="font-mono text-sm">
          {tempHigh != null && <span className="text-neon-red">↑ {tempHigh}°C</span>}
          {tempHigh != null && tempLow != null && <span className="text-gray-500"> / </span>}
          {tempLow != null && <span className="text-neon-cyan">↓ {tempLow}°C</span>}
        </p>
      )}
      {precip != null && (
        <p className="text-xs text-gray-400">Precipitation: {precip}%</p>
      )}
      {humidity != null && (
        <p className="text-xs text-gray-400">Humidity: {humidity}%</p>
      )}
      {wind != null && (
        <p className="text-xs text-gray-400">
          Wind: {wind} m/s{windDir ? ` ${windDir}` : ''}
        </p>
      )}
      {overview && (
        <div className="mt-2 pt-2 border-t border-osint-border/50">
          <p className="text-xs text-gray-300 leading-snug whitespace-pre-wrap">{overview}</p>
        </div>
      )}
      <PropertyTable properties={properties} exclude={highlighted} />
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

  const highlighted = ['aqi', 'value', 'station', 'name'];
  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{properties.station || properties.name || 'Station'}</p>
      <div className="flex items-center gap-2">
        <span className="text-2xl font-mono font-bold" style={{ color }}>{aqi ?? '?'}</span>
        <span className="text-xs px-2 py-0.5 rounded" style={{ background: color + '22', color }}>{label}</span>
      </div>
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function RadiationDetail({ properties }) {
  const value = properties.value ?? properties.nGy;
  let color = '#00ff88';
  if (value > 100) color = '#ff4444';
  else if (value > 50) color = '#ffd600';
  const highlighted = ['value', 'nGy', 'station', 'name'];

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium">{properties.station || properties.name || 'Station'}</p>
      <p className="font-mono text-xl font-bold" style={{ color }}>
        {value ?? '?'} <span className="text-xs text-gray-400">nGy/h</span>
      </p>
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function RiverDetail({ properties }) {
  const level = properties.water_level_m ?? properties.water_level ?? properties.level;
  const warn = properties.warning_level_m;
  const danger = properties.danger_level_m;
  const status = properties.status;
  const statusColors = {
    danger: '#ff4444',
    warning: '#ff8c00',
    attention: '#ffd600',
    normal: '#00ff88',
  };
  const color = statusColors[status] || '#4fc3f7';

  const highlighted = [
    'water_level_m', 'water_level', 'level',
    'warning_level_m', 'danger_level_m', 'status',
    'station_name', 'river_name', 'prefecture',
  ];

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium text-gray-200">
        {properties.station_name || 'River gauge'}
        {properties.river_name && (
          <span className="text-xs text-gray-400 ml-2">{properties.river_name}</span>
        )}
      </p>
      {level != null && (
        <p className="font-mono text-xl font-bold" style={{ color }}>
          {level} <span className="text-xs text-gray-400">m</span>
        </p>
      )}
      {status && (
        <span
          className="inline-block text-xs px-2 py-0.5 rounded font-mono uppercase"
          style={{ background: color + '22', color }}
        >
          {status}
        </span>
      )}
      {(warn != null || danger != null) && (
        <div className="text-xs text-gray-400 space-x-3">
          {warn != null && <span>Warning: {warn} m</span>}
          {danger != null && <span>Danger: {danger} m</span>}
        </div>
      )}
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function GenericDetail({ properties }) {
  // Prefer showing a human title if the feature has one.
  const title =
    properties.name ||
    properties.title ||
    properties.station_name ||
    properties.prefecture_name ||
    properties.place ||
    null;

  const highlighted = title
    ? ['name', 'title', 'station_name', 'prefecture_name', 'place']
    : [];

  return (
    <div className="space-y-1">
      {title && <p className="text-sm font-medium text-gray-200">{title}</p>}
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

const DETAIL_RENDERERS = {
  earthquakes: EarthquakeDetail,
  cameras: CameraDetail,
  weather: WeatherDetail,
  airQuality: AirQualityDetail,
  radiation: RadiationDetail,
  river: RiverDetail,
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
      className="absolute z-40 glass-panel p-3 min-w-[240px] max-w-[340px] shadow-lg"
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
      <ReverseGeocodeLabel feature={feature} />
    </div>
  );
}
