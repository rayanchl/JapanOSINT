import React, { useEffect, useState } from 'react';
import { MdClose } from 'react-icons/md';
import { getLayerIcon } from '../../utils/layerIcons';
import { LAYER_DEFINITIONS } from '../../hooks/useMapLayers';

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

/**
 * Extract a YouTube video ID from common URL forms.
 * Returns null when the URL is not a recognised YouTube link.
 */
function youtubeVideoId(url) {
  if (!url) return null;
  // youtube.com/watch?v=ID  |  youtu.be/ID  |  youtube.com/embed/ID
  const m = url.match(
    /(?:youtube\.com\/(?:watch\?.*v=|embed\/)|youtu\.be\/)([\w-]{11})/
  );
  return m ? m[1] : null;
}

/**
 * Heuristic: does this URL point directly at a JPEG/MJPEG snapshot we can
 * render as an <img>? Covers Insecam previews, shutoko.jp thumbnails, etc.
 */
function isImageUrl(url) {
  if (!url) return false;
  return /\.(jpe?g|png|gif|bmp|webp)(\?.*)?$/i.test(url)
    || /mjpg|snapshot|image\.cgi|\/camera\//i.test(url);
}

/**
 * Hosts whose live-cam pages are designed to be iframe-embedded
 * (player loads inside the page itself, X-Frame-Options permissive).
 */
// Hosts whose pages can actually be iframed (permissive X-Frame-Options /
// no frame-ancestors CSP). Skylinewebcams and EarthCam are NOT listed here:
// their pages send X-Frame-Options: DENY, so we route them to the server-
// side Chromium snapshot pipeline instead of trying to iframe.
const IFRAMEABLE_CAM_HOSTS = [
  'river.go.jp',
  'www.river.go.jp',
  'windy.com',
  'www.windy.com',
  'livecam.asia',
  'www.livecam.asia',
];
function iframeableCamUrl(url, channel) {
  if (!url) return null;
  if (channel === 'mlit_river') return url;
  try {
    const host = new URL(url).hostname.toLowerCase();
    if (IFRAMEABLE_CAM_HOSTS.includes(host)) return url;
  } catch { /* invalid url */ }
  return null;
}

/**
 * Discovery channels whose URLs we know are embed-blocked (X-Frame-Options
 * DENY / CSP frame-ancestors). The popup renders a server-captured snapshot
 * via /api/data/cameras/snapshot for these.
 */
const SNAPSHOT_CHANNELS = new Set([
  'skylinewebcams',
  'earthcam',
  'webcamtaxi',
  'geocam',
  'worldcams',
  'webcamera24',
  'camstreamer',
]);

function CameraSnapshot({ url, name }) {
  const [state, setState] = useState('loading'); // loading | ok | failed
  const src = `/api/data/cameras/snapshot?url=${encodeURIComponent(url)}`;
  return (
    <div className="relative w-full aspect-video rounded border border-osint-border overflow-hidden bg-osint-panel">
      {state !== 'failed' && (
        <img
          src={src}
          alt={name}
          loading="lazy"
          className="w-full h-full object-cover"
          onLoad={() => setState('ok')}
          onError={() => setState('failed')}
          style={state === 'loading' ? { opacity: 0 } : undefined}
        />
      )}
      {state === 'loading' && (
        <div className="absolute inset-0 flex items-center justify-center text-[11px] text-gray-400">
          Capturing snapshot…
        </div>
      )}
      {state === 'failed' && (
        <div className="absolute inset-0 flex items-center justify-center text-[11px] text-gray-500">
          Snapshot unavailable
        </div>
      )}
    </div>
  );
}

function CameraDetail({ properties }) {
  const name = properties.name || properties.station || 'Camera';
  const operator = properties.operator;
  const status = properties.status;
  const thumb = properties.thumbnail_url || properties.thumbnail;
  const streamUrl = properties.stream_url || properties.url;
  const channel = properties.discovery_channel;

  // Purple tag shows the source (discovery channel).
  const sourceColor = '#ce93d8';

  const highlighted = [
    'name', 'station', 'thumbnail', 'thumbnail_url', 'location', 'url',
    'stream_url', 'camera_type', 'type', 'surveillance_type', 'operator',
    'status', 'camera_id', 'camera_uid', 'source', 'discovery_channel',
    'discovery_channels', 'country', 'id',
  ];

  // --- Determine what we can embed ---
  const ytId = youtubeVideoId(streamUrl);
  const iframeSrc = iframeableCamUrl(streamUrl, channel);
  // Prefer thumbnail, then fall back to stream_url if it looks like an image
  const imgSrc = thumb || (isImageUrl(streamUrl) ? streamUrl : null);
  // Embed-blocked sources: fetch a server-captured JPEG snapshot.
  const useSnapshot = !ytId && !iframeSrc && !imgSrc
    && streamUrl && SNAPSHOT_CHANNELS.has(channel);

  return (
    <div className="space-y-2">
      <p className="text-sm font-medium text-gray-200">{name}</p>
      <div className="flex items-center gap-2">
        {channel && (
          <span
            className="inline-block text-[10px] px-1.5 py-0.5 rounded font-mono uppercase"
            style={{ background: sourceColor + '22', color: sourceColor }}
          >
            {channel}
          </span>
        )}
        {operator && (
          <span className="text-xs text-gray-400">{operator}</span>
        )}
      </div>

      {/* ── Embedded feed ──────────────────────────────────────── */}
      {ytId ? (
        <iframe
          src={`https://www.youtube.com/embed/${ytId}?autoplay=1&mute=1`}
          title={name}
          className="w-full aspect-video rounded border border-osint-border"
          allow="autoplay; encrypted-media"
          allowFullScreen
        />
      ) : iframeSrc ? (
        <iframe
          src={iframeSrc}
          title={name}
          className="w-full aspect-video rounded border border-osint-border"
          allow="autoplay; fullscreen"
          loading="lazy"
          referrerPolicy="no-referrer-when-downgrade"
        />
      ) : imgSrc ? (
        <img
          src={imgSrc}
          alt={name}
          className="w-full rounded border border-osint-border"
          loading="lazy"
        />
      ) : useSnapshot ? (
        <CameraSnapshot url={streamUrl} name={name} />
      ) : null}

      {properties.location && (
        <p className="text-xs text-gray-400">{properties.location}</p>
      )}
      {streamUrl && (
        <a
          href={streamUrl}
          target="_blank"
          rel="noopener noreferrer"
          className="text-xs text-neon-cyan hover:underline"
        >
          {ytId ? 'Open on YouTube' : 'Open Feed'}
        </a>
      )}
      {status && (
        <p className="text-xs text-gray-500">Status: {status}</p>
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

function FlightDetail({ properties }) {
  const callsign = properties.callsign;
  const icao24 = properties.icao24;
  const airline = properties.airline;
  const aircraftType = properties.aircraft_type;
  const category = properties.category;
  const origin = properties.origin;
  const destination = properties.destination;
  const originCountry = properties.origin_country;
  const squawk = properties.squawk;
  const altFt = properties.altitude_ft;
  const geoAltFt = properties.geo_altitude_ft;
  const speed = properties.ground_speed_knots ?? properties.speed_knots;
  const heading = properties.heading ?? properties.true_track;
  const vertRate = properties.vertical_rate_fpm ?? properties.vertical_rate;
  const onGround = properties.on_ground;
  const posSrc = properties.position_source;

  const coords = properties._coords; // injected below if available

  const highlighted = [
    'callsign', 'icao24', 'airline', 'aircraft_type', 'category',
    'origin', 'destination', 'origin_country', 'squawk',
    'altitude_ft', 'baro_altitude_m', 'geo_altitude_ft', 'geo_altitude_m',
    'ground_speed_knots', 'speed_knots', 'velocity_mps',
    'heading', 'true_track', 'vertical_rate_fpm', 'vertical_rate',
    'on_ground', 'position_source', 'spi', 'id',
    'time_position', 'last_contact', 'last_update', 'status', 'source',
  ];

  // Vertical rate arrow
  let vertArrow = '';
  if (vertRate != null && vertRate !== 0) {
    vertArrow = vertRate > 0 ? ' \u2191' : ' \u2193';
  }

  return (
    <div className="space-y-2">
      {/* Header: callsign + icao24 */}
      <div className="flex items-center gap-2">
        <span className="text-lg font-mono font-bold text-neon-cyan">
          {callsign || icao24 || '?'}
        </span>
        {callsign && icao24 && (
          <span className="text-xs text-gray-500 font-mono">{icao24}</span>
        )}
        {onGround && (
          <span className="text-[10px] px-1.5 py-0.5 rounded bg-green-900/40 text-green-400 uppercase">
            Ground
          </span>
        )}
      </div>

      {/* Aircraft info line */}
      {(airline || aircraftType || category) && (
        <div className="text-xs text-gray-400 space-x-2">
          {airline && <span>{airline}</span>}
          {aircraftType && <span className="font-mono">{aircraftType}</span>}
          {category && category !== 'No info' && category !== 'No ADS-B category' && (
            <span className="text-gray-500">({category})</span>
          )}
        </div>
      )}

      {/* Route */}
      {(origin || destination) && (
        <p className="text-sm text-gray-300 font-mono">
          {origin || '???'} &rarr; {destination || '???'}
        </p>
      )}
      {originCountry && !origin && (
        <p className="text-xs text-gray-400">Origin: {originCountry}</p>
      )}

      {/* Key metrics grid */}
      <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-xs">
        {altFt != null && (
          <>
            <span className="text-gray-500">Baro Alt</span>
            <span className="text-gray-200 font-mono text-right">{altFt.toLocaleString()} ft</span>
          </>
        )}
        {geoAltFt != null && (
          <>
            <span className="text-gray-500">WGS-84 Alt</span>
            <span className="text-gray-200 font-mono text-right">{geoAltFt.toLocaleString()} ft</span>
          </>
        )}
        {speed != null && (
          <>
            <span className="text-gray-500">Ground Speed</span>
            <span className="text-gray-200 font-mono text-right">{speed} kts</span>
          </>
        )}
        {heading != null && (
          <>
            <span className="text-gray-500">Track</span>
            <span className="text-gray-200 font-mono text-right">{heading}&deg;</span>
          </>
        )}
        {vertRate != null && vertRate !== 0 && (
          <>
            <span className="text-gray-500">Vert Rate</span>
            <span className="text-gray-200 font-mono text-right">
              {vertRate > 0 ? '+' : ''}{vertRate} fpm{vertArrow}
            </span>
          </>
        )}
        {squawk && (
          <>
            <span className="text-gray-500">Squawk</span>
            <span className={`font-mono text-right ${squawk === '7700' ? 'text-neon-red font-bold' : squawk === '7600' ? 'text-neon-orange font-bold' : squawk === '7500' ? 'text-neon-red font-bold' : 'text-gray-200'}`}>
              {squawk}
              {squawk === '7700' && ' EMERGENCY'}
              {squawk === '7600' && ' RADIO FAIL'}
              {squawk === '7500' && ' HIJACK'}
            </span>
          </>
        )}
        {posSrc && (
          <>
            <span className="text-gray-500">Source</span>
            <span className="text-gray-200 font-mono text-right">{posSrc}</span>
          </>
        )}
      </div>

      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function TwitterGeoDetail({ properties }) {
  const platform = properties.platform || 'twitter';
  const username = properties.username;
  const displayName = properties.display_name;
  const text = properties.text;
  const timestamp = properties.timestamp;
  const url = properties.url;
  const instance = properties.instance;
  const verified = properties.verified;
  const isSeed = properties.source === 'twitter_seed';

  const handle = username
    ? (platform === 'mastodon'
        ? `@${username}${username.includes('@') ? '' : (instance ? `@${instance}` : '')}`
        : `@${username}`)
    : null;

  const platformColor = platform === 'mastodon' ? '#6364ff' : '#1da1f2';
  const platformLabel = platform === 'mastodon' ? 'Mastodon' : 'Twitter/X';

  const highlighted = [
    'platform', 'username', 'display_name', 'text', 'timestamp',
    'url', 'instance', 'verified', 'id', 'source',
  ];

  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2 flex-wrap">
        <span
          className="text-[10px] px-1.5 py-0.5 rounded font-mono uppercase"
          style={{ background: platformColor + '22', color: platformColor }}
        >
          {platformLabel}
        </span>
        {handle && (
          <span className="text-sm font-medium text-gray-200 break-all">
            {displayName ? `${displayName} ` : ''}
            <span className="text-gray-400 font-mono text-xs">{handle}</span>
          </span>
        )}
        {verified && (
          <span className="text-[10px] px-1 py-0.5 rounded bg-blue-900/40 text-blue-300">
            verified
          </span>
        )}
      </div>

      {text && (
        <p className="text-sm text-gray-200 leading-snug whitespace-pre-wrap break-words">
          {text}
        </p>
      )}

      {timestamp && (
        <p className="text-xs text-gray-500 font-mono">
          {formatTimestamp(timestamp)}
        </p>
      )}

      {url ? (
        <a
          href={url}
          target="_blank"
          rel="noopener noreferrer"
          className="inline-block text-xs text-neon-cyan hover:underline"
        >
          Open original post &rarr;
        </a>
      ) : isSeed ? (
        <p className="text-[10px] text-gray-600 italic">Seed sample (no live link)</p>
      ) : null}

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

function SatelliteImageryDetail({ properties }) {
  const [baked, setBaked] = useState(false);
  const [opacity, setOpacity] = useState(0.6);

  const sceneId = properties.scene_id;
  const platform = properties.platform;
  const tileUrl = properties.tile_url;
  const previewUrl = properties.preview_url;
  const hasFeed = !!(tileUrl || previewUrl);

  useEffect(() => {
    window.dispatchEvent(new CustomEvent('satellite-imagery-bake', {
      detail: {
        show: baked,
        sceneId,
        platform,
        tileUrl,
        previewUrl,
        opacity,
      },
    }));
  }, [baked, opacity, sceneId, platform, tileUrl, previewUrl]);

  useEffect(() => {
    return () => {
      window.dispatchEvent(new CustomEvent('satellite-imagery-bake', {
        detail: { show: false, sceneId: properties.scene_id || properties.id },
      }));
    };
  }, []);

  const highlighted = [
    'platform', 'sensor', 'scene_id', 'datetime',
    'cloud_cover', 'preview_url', 'tile_url', 'archive_era', 'source',
  ];
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-sm font-medium text-gray-200">
          {properties.platform}
        </span>
        {properties.sensor && (
          <span className="text-[10px] px-1.5 py-0.5 rounded bg-neon-cyan/20 text-neon-cyan font-mono">
            {properties.sensor}
          </span>
        )}
        {properties.archive_era === 'historical' && (
          <span className="text-[10px] px-1.5 py-0.5 rounded bg-amber-500/20 text-amber-400 font-mono">
            archive
          </span>
        )}
      </div>
      {properties.datetime && (
        <p className="text-xs text-gray-500 font-mono">
          {formatTimestamp(properties.datetime)}
          {properties.cloud_cover != null && (
            <span className="ml-2">cloud {properties.cloud_cover}%</span>
          )}
        </p>
      )}
      {properties.preview_url && (
        <img
          src={properties.preview_url}
          alt={`${properties.platform} preview`}
          style={{ maxWidth: 240, maxHeight: 180, objectFit: 'contain' }}
          className="rounded border border-osint-border/50"
          onError={(e) => { e.currentTarget.style.display = 'none'; }}
        />
      )}
      {hasFeed && (
        <label className="flex items-center gap-2 cursor-pointer select-none">
          <input
            type="checkbox"
            checked={baked}
            onChange={(e) => setBaked(e.target.checked)}
            className="accent-neon-cyan"
          />
          <span className="text-xs text-gray-300">Bake feed on map</span>
        </label>
      )}
      {baked && (
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-500">Opacity</span>
          <input
            type="range"
            min={0}
            max={1}
            step={0.05}
            value={opacity}
            onChange={(e) => setOpacity(Number(e.target.value))}
            className="flex-1 accent-neon-cyan"
          />
          <span className="text-xs text-gray-400 font-mono w-8 text-right">
            {Math.round(opacity * 100)}%
          </span>
        </div>
      )}
      <p className="text-[10px] text-gray-600 font-mono">src: {properties.source}</p>
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}

function SatelliteTrackingDetail({ properties }) {
  const [showTrack, setShowTrack] = useState(false);

  useEffect(() => {
    return () => {
      window.dispatchEvent(new CustomEvent('satellite-track-toggle', {
        detail: { show: false, noradId: properties.norad_id },
      }));
    };
  }, []);
  const highlighted = [
    'name', 'norad_id', 'category', 'altitude_km', 'velocity_kms',
    'inclination_deg', 'next_pass_utc', 'tle_line1', 'tle_line2', 'source',
  ];
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-sm font-medium text-gray-200">{properties.name}</span>
        <span className="text-[10px] px-1.5 py-0.5 rounded bg-purple-500/20 text-purple-300 font-mono">
          {properties.category}
        </span>
      </div>
      <div className="text-xs text-gray-400 space-y-0.5 font-mono">
        <div>NORAD: {properties.norad_id}</div>
        {properties.altitude_km != null && <div>Altitude: {properties.altitude_km} km</div>}
        {properties.velocity_kms != null && <div>Velocity: {properties.velocity_kms} km/s</div>}
        {properties.inclination_deg != null && <div>Inclination: {properties.inclination_deg}°</div>}
      </div>
      <button
        type="button"
        className="text-xs px-2 py-1 rounded bg-neon-cyan/20 text-neon-cyan hover:bg-neon-cyan/30 transition"
        onClick={() => {
          setShowTrack((v) => !v);
          // Dispatch a custom event so MapView picks it up and draws / clears
          // the ground-track layer. Keeping MapPopup decoupled from map state.
          const evt = new CustomEvent('satellite-track-toggle', {
            detail: {
              noradId: properties.norad_id,
              tleLine1: properties.tle_line1,
              tleLine2: properties.tle_line2,
              show: !showTrack,
            },
          });
          window.dispatchEvent(evt);
        }}
      >
        {showTrack ? 'Hide ground track' : 'Show ground track'}
      </button>
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
  flightAdsb: FlightDetail,
  'flight-adsb': FlightDetail,
  twitterGeo: TwitterGeoDetail,
  'twitter-geo': TwitterGeoDetail,
  satelliteImagery: SatelliteImageryDetail,
  'satellite-imagery': SatelliteImageryDetail,
  satelliteTracking: SatelliteTrackingDetail,
  'satellite-tracking': SatelliteTrackingDetail,
};

export default function MapPopup({ feature, layerType, onClose, position }) {
  if (!feature) return null;

  const properties = feature.properties || feature;
  const Renderer = DETAIL_RENDERERS[layerType] || GenericDetail;
  const layerDef = layerType ? LAYER_DEFINITIONS[layerType] : null;
  const LayerIcon = layerType ? getLayerIcon(layerType) : null;
  const iconColor = layerDef?.color || '#22d3ee';
  const layerLabel = layerDef?.name
    || (layerType
      ? layerType.replace(/([A-Z])/g, ' $1').replace(/^./, (s) => s.toUpperCase())
      : 'Feature');

  return (
    <div
      className="absolute z-40 glass-panel map-popup-ridge p-3 min-w-[240px] max-w-[340px] shadow-lg"
      style={{
        left: position?.x ?? 0,
        top: position?.y ?? 0,
        transform: 'translate(-50%, -110%)',
      }}
    >
      <div className="flex items-center justify-between mb-2">
        <span className="flex items-center gap-1.5 text-[10px] uppercase tracking-wider text-neon-cyan font-medium">
          {LayerIcon && (
            <LayerIcon size={16} color={iconColor} aria-hidden="true" />
          )}
          {layerLabel}
        </span>
        <button
          onClick={onClose}
          className="text-gray-500 hover:text-gray-200 ml-2 p-0.5 rounded hover:bg-osint-border/40"
          aria-label="Close popup"
        >
          <MdClose size={14} aria-hidden="true" />
        </button>
      </div>
      <Renderer
        key={properties.id || properties.scene_id || properties.norad_id || properties.gs_id}
        properties={properties}
        layerType={layerType}
      />
      <ReverseGeocodeLabel feature={feature} />
    </div>
  );
}
