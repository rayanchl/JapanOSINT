import React, { useMemo } from 'react';
import useCameraDiscoveryStream from '../../hooks/useCameraDiscoveryStream';

function flyTo(lat, lon) {
  window.dispatchEvent(
    new CustomEvent('japanosint:flyto', { detail: { lat, lon, zoom: 13 } }),
  );
}

function fmtElapsed(ms) {
  if (!Number.isFinite(ms)) return '—';
  const s = Math.floor(ms / 1000);
  const mm = Math.floor(s / 60).toString().padStart(2, '0');
  const ss = (s % 60).toString().padStart(2, '0');
  return `${mm}:${ss}`;
}

function KindBadge({ kind }) {
  if (kind === 'new') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-status-online/15 text-status-online border border-status-online/30">
        NEW
      </span>
    );
  }
  return (
    <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-neon-cyan/10 text-neon-cyan border border-neon-cyan/30">
      UPDATED
    </span>
  );
}

function ChannelChip({ channel }) {
  return (
    <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-osint-bg/80 text-gray-300 border border-osint-border/60">
      {channel}
    </span>
  );
}

function EventRow({ ev }) {
  const p = ev.camera?.properties || {};
  const coords = ev.camera?.geometry?.coordinates || [];
  const [lon, lat] = coords;
  const thumb = p.thumbnail_url;
  return (
    <div className="border border-osint-border/40 rounded bg-osint-bg/40 px-2 py-1.5 flex gap-2">
      {thumb ? (
        <img
          src={thumb}
          alt=""
          className="w-12 h-12 object-cover rounded border border-osint-border/40 flex-shrink-0"
          loading="lazy"
          onError={(e) => { e.currentTarget.style.display = 'none'; }}
        />
      ) : null}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1 flex-wrap">
          <KindBadge kind={ev.kind} />
          <ChannelChip channel={ev.channel} />
          <span className="text-[9px] text-gray-500 font-mono ml-auto">
            {new Date(ev.ts).toLocaleTimeString('en-GB')}
          </span>
        </div>
        <div className="text-[11px] text-gray-200 truncate mt-0.5" title={p.name}>
          {p.name || 'Unknown camera'}
        </div>
        <div className="flex items-center gap-2 mt-0.5">
          <span className="text-[9px] text-gray-500 font-mono">
            {Number.isFinite(lat) ? lat.toFixed(4) : '—'}, {Number.isFinite(lon) ? lon.toFixed(4) : '—'}
          </span>
          {Number.isFinite(lat) && Number.isFinite(lon) && (
            <button
              type="button"
              onClick={() => flyTo(lat, lon)}
              className="text-[9px] text-neon-cyan hover:underline font-mono"
            >
              View on map →
            </button>
          )}
          {p.url && (
            <a
              href={p.url}
              target="_blank"
              rel="noreferrer"
              className="text-[9px] text-gray-400 hover:text-neon-cyan font-mono truncate"
              title={p.url}
            >
              ↗ source
            </a>
          )}
        </div>
      </div>
    </div>
  );
}

function RunBanner({ activeRun, lastRun }) {
  if (activeRun) {
    const channelsDone = activeRun.channels_done?.length ?? 0;
    return (
      <div className="px-3 py-2 border-b border-osint-border/40 bg-neon-cyan/5">
        <div className="flex items-center justify-between text-[10px]">
          <div className="flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-status-online pulse-live" />
            <span className="text-neon-cyan font-mono">RUN ACTIVE</span>
            <span className="text-gray-400 font-mono">{activeRun.run_id.slice(11, 19)}</span>
          </div>
          <span className="font-mono text-gray-300">{fmtElapsed(activeRun.elapsed_ms)}</span>
        </div>
        <div className="mt-1 text-[10px] text-gray-400 font-mono flex flex-wrap gap-x-3 gap-y-0.5">
          <span>
            <span className="text-status-online">{activeRun.new_count}</span> new
          </span>
          <span>
            <span className="text-neon-cyan">{activeRun.updated_count}</span> updated
          </span>
          <span>{channelsDone}/15 channels done</span>
        </div>
      </div>
    );
  }
  if (lastRun) {
    return (
      <div className="px-3 py-2 border-b border-osint-border/40">
        <div className="text-[10px] text-gray-400 font-mono flex flex-wrap gap-x-3">
          <span className="text-gray-500">Last run</span>
          <span>
            <span className="text-status-online">+{lastRun.new_count}</span> new,{' '}
            <span className="text-neon-cyan">{lastRun.updated_count}</span> updated
          </span>
          <span>in {fmtElapsed(lastRun.elapsed_ms)}</span>
          <span className="text-gray-500">DB total {lastRun.db_total}</span>
        </div>
      </div>
    );
  }
  return (
    <div className="px-3 py-2 border-b border-osint-border/40 text-[10px] text-gray-500 italic">
      Waiting for first camera run…
    </div>
  );
}

export default function CameraDiscoveryThread() {
  const { events, activeRun, lastRun, connected, clearEvents } = useCameraDiscoveryStream();

  const header = useMemo(() => {
    const run = activeRun;
    if (!run) return null;
    const entries = Object.entries(run.run_counts || {});
    entries.sort((a, b) => b[1] - a[1]);
    return entries;
  }, [activeRun]);

  return (
    <div className="flex flex-col h-full">
      <RunBanner activeRun={activeRun} lastRun={lastRun} />

      {header && header.length > 0 && (
        <div className="px-3 py-1.5 border-b border-osint-border/40 flex flex-wrap gap-1">
          {header.map(([ch, n]) => (
            <span
              key={ch}
              className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-osint-bg/60 text-gray-300 border border-osint-border/50"
              title={ch}
            >
              {ch}: <span className="text-neon-green">{n}</span>
            </span>
          ))}
        </div>
      )}

      <div className="flex items-center justify-between px-3 py-1.5 border-b border-osint-border/40 text-[10px]">
        <span className="text-gray-500">
          {events.length} event{events.length === 1 ? '' : 's'}
          {!connected && <span className="ml-2 text-status-offline">· WS offline</span>}
        </span>
        {events.length > 0 && (
          <button
            type="button"
            onClick={clearEvents}
            className="text-gray-500 hover:text-neon-cyan font-mono"
          >
            clear
          </button>
        )}
      </div>

      <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1 min-h-0">
        {events.length === 0 && (
          <div className="text-gray-500 text-xs px-2 py-6 text-center italic">
            {activeRun
              ? 'Scanning channels… features will stream in as they arrive.'
              : 'No discoveries yet. The next run is scheduled hourly, or triggers on server boot.'}
          </div>
        )}
        {events.map((ev, i) => (
          <EventRow key={`${ev.run_id}:${ev.camera?.properties?.camera_uid}:${i}`} ev={ev} />
        ))}
      </div>
    </div>
  );
}
