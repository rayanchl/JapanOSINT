import { useEffect, useState } from 'react';
import LineChip from './LineChip.jsx';
import DeparturesBoard from './DeparturesBoard.jsx';

export default function StationPopup({ properties }) {
  const stationUid = properties?.station_uid || properties?.stationUid;
  const [data, setData] = useState(null);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (!stationUid) { setError('No stationUid'); return; }
    let cancelled = false;
    (async () => {
      try {
        const r = await fetch(`/api/transit/station/${encodeURIComponent(stationUid)}/summary`);
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        const body = await r.json();
        if (!cancelled) setData(body);
      } catch (e) {
        if (!cancelled) setError(e.message);
      }
    })();
    return () => { cancelled = true; };
  }, [stationUid]);

  if (error) return <div className="text-xs text-red-400">{error}</div>;
  if (!data) return <div className="text-xs text-gray-500">Loading…</div>;

  const now = new Date();
  const nowSec = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds();

  const isCluster = !!data.station?.cluster_uid;
  const memberCount = data.station?.member_count;

  return (
    <div className="space-y-2">
      <div className="flex items-baseline justify-between gap-2">
        <div className="text-sm font-medium text-gray-200">{data.station.name}</div>
        {isCluster && memberCount > 1 && (
          <span className="text-[9px] uppercase tracking-wider text-gray-500">
            {memberCount} platforms
          </span>
        )}
      </div>
      {data.station.name_ja && data.station.name_ja !== data.station.name && (
        <div className="text-[11px] text-gray-400">{data.station.name_ja}</div>
      )}
      {data.station.operator && (
        <div className="text-[10px] text-gray-500 font-mono">{data.station.operator}</div>
      )}
      {data.lines.length > 0 && (
        <div className="flex flex-wrap gap-1">
          {data.lines.map((l, i) => (
            <LineChip key={i} color={l.color} refText={l.ref} name={l.name} />
          ))}
        </div>
      )}
      {data.alerts.length > 0 && (
        <div className="text-xs text-amber-300 bg-amber-900/30 rounded px-1.5 py-1 space-y-0.5">
          {data.alerts.slice(0, 3).map((a, i) => (
            <div key={i}>{a.header_text || a.description_text}</div>
          ))}
        </div>
      )}
      <DeparturesBoard
        title="Departures"
        rows={data.departures}
        emptyMsg="No upcoming departures today."
        nowSec={nowSec}
      />
      <DeparturesBoard
        title="Arrivals"
        rows={data.arrivals}
        emptyMsg="No upcoming arrivals today."
        nowSec={nowSec}
      />
    </div>
  );
}
