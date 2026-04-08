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
  const name = properties.prefecture_name || properties.station || properties.name || 'Station';
  const condition = properties.weather_condition || properties.condition || '';
  const tempHigh = properties.temperature_high ?? properties.temperature;
  const tempLow = properties.temperature_low;
  const humidity = properties.humidity_percent ?? properties.humidity;
  const windSpeed = properties.wind_speed_ms ?? properties.wind_speed;
  const windDir = properties.wind_direction || '';
  const precip = properties.precipitation_probability;

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium text-gray-200">{name}</p>
      {condition && <p className="text-neon-cyan text-base">{condition}</p>}
      {tempHigh != null && (
        <div className="font-mono text-lg">
          <span className="text-red-400">{tempHigh}°</span>
          {tempLow != null && <span className="text-blue-400 ml-1">/ {tempLow}°</span>}
        </div>
      )}
      {humidity != null && (
        <p className="text-xs text-gray-400">Humidity: {humidity}%</p>
      )}
      {windSpeed != null && (
        <p className="text-xs text-gray-400">Wind: {windDir} {windSpeed} m/s</p>
      )}
      {precip != null && (
        <p className="text-xs text-gray-400">Rain probability: {precip}%</p>
      )}
      {properties.weather_overview && (
        <p className="text-xs text-gray-400 mt-1 line-clamp-3">{properties.weather_overview}</p>
      )}
    </div>
  );
}

function AirQualityDetail({ properties }) {
  const pm25 = properties.pm25_ugm3 ?? properties.pm25;
  const aqi = properties.aqi ?? properties.value;
  const category = properties.aqi_category || '';
  const name = properties.station_name || properties.station || properties.name || 'Station';
  const pref = properties.prefecture || '';

  // Color based on PM2.5 or AQI
  const val = pm25 ?? aqi ?? 0;
  let color = '#00ff88';
  let label = category || 'Good';
  if (val > 55) { color = '#ff4444'; label = category || 'Unhealthy'; }
  else if (val > 35) { color = '#ff8c00'; label = category || 'Unhealthy for Sensitive'; }
  else if (val > 12) { color = '#ffb74d'; label = category || 'Moderate'; }

  return (
    <div className="space-y-2">
      <p className="text-sm font-medium text-gray-200">{name}</p>
      {pref && <p className="text-xs text-gray-500">{pref}</p>}
      <div className="flex items-center gap-2">
        <span className="text-2xl font-mono font-bold" style={{ color }}>{pm25 ?? aqi ?? '?'}</span>
        <div>
          <span className="text-xs px-2 py-0.5 rounded block" style={{ background: color + '22', color }}>{label}</span>
          <span className="text-[10px] text-gray-500 mt-0.5 block">PM2.5 µg/m³</span>
        </div>
      </div>
      <div className="grid grid-cols-2 gap-x-3 gap-y-1 text-xs">
        {properties.pm10_ugm3 != null && (
          <><span className="text-gray-400">PM10</span><span className="font-mono text-gray-200">{properties.pm10_ugm3} µg/m³</span></>
        )}
        {properties.no2_ppb != null && (
          <><span className="text-gray-400">NO₂</span><span className="font-mono text-gray-200">{properties.no2_ppb} ppb</span></>
        )}
        {properties.so2_ppb != null && (
          <><span className="text-gray-400">SO₂</span><span className="font-mono text-gray-200">{properties.so2_ppb} ppb</span></>
        )}
        {properties.ox_ppb != null && (
          <><span className="text-gray-400">Ox</span><span className="font-mono text-gray-200">{properties.ox_ppb} ppb</span></>
        )}
        {properties.co_ppm != null && (
          <><span className="text-gray-400">CO</span><span className="font-mono text-gray-200">{properties.co_ppm} ppm</span></>
        )}
      </div>
    </div>
  );
}

function RadiationDetail({ properties }) {
  const value = properties.dose_rate_nGyh ?? properties.value ?? properties.nGy;
  const name = properties.station_name || properties.station || properties.name || 'Station';
  const pref = properties.prefecture || '';
  const status = properties.status || '';

  let color = '#00ff88';
  let statusLabel = 'Normal';
  if (value > 200) { color = '#ff4444'; statusLabel = 'Elevated'; }
  else if (value > 100) { color = '#ff8c00'; statusLabel = 'Attention'; }
  else if (value > 50) { color = '#ffd600'; statusLabel = 'Monitoring'; }

  return (
    <div className="space-y-1">
      <p className="text-sm font-medium text-gray-200">{name}</p>
      {pref && <p className="text-xs text-gray-500">{pref}</p>}
      <p className="font-mono text-xl font-bold" style={{ color }}>
        {value != null ? value.toFixed(1) : '?'} <span className="text-xs text-gray-400">nGy/h</span>
      </p>
      <span className="text-xs px-2 py-0.5 rounded" style={{ background: color + '22', color }}>
        {status || statusLabel}
      </span>
      {properties.measured_at && (
        <p className="text-[10px] text-gray-500 mt-1 font-mono">
          {new Date(properties.measured_at).toLocaleString('en-GB', { timeZone: 'Asia/Tokyo' })} JST
        </p>
      )}
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
