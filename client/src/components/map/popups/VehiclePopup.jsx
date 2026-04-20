import { useEffect, useState } from 'react';
import LineChip from './LineChip.jsx';

export default function VehiclePopup({ properties }) {
  const tripId = properties?.trip_id;
  const [data, setData] = useState(null);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (!tripId) { setError('Not a schedule-backed vehicle'); return; }
    let cancelled = false;
    (async () => {
      try {
        const r = await fetch(`/api/transit/vehicle/${encodeURIComponent(tripId)}/info`);
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        const body = await r.json();
        if (!cancelled) setData(body);
      } catch (e) {
        if (!cancelled) setError(e.message);
      }
    })();
    return () => { cancelled = true; };
  }, [tripId]);

  if (error) return <div className="text-xs text-gray-500">{error}</div>;
  if (!data) return <div className="text-xs text-gray-500">Loading…</div>;

  const delayMin = data.next_stop?.delay_s != null
    ? Math.round(data.next_stop.delay_s / 60)
    : null;

  return (
    <div className="space-y-2">
      <div className="flex items-center gap-1.5">
        <LineChip
          color={data.trip.route_color}
          refText={data.trip.route_short}
          name={data.trip.route_long}
        />
        {data.trip.headsign && (
          <span className="text-sm text-gray-200 truncate">→ {data.trip.headsign}</span>
        )}
      </div>
      {data.next_stop && (
        <div className="text-xs text-gray-400">
          Next: <span className="text-gray-200">{data.next_stop.stop_id}</span>
          {delayMin != null && Math.abs(delayMin) > 0 && (
            <span className={`ml-2 font-mono ${delayMin > 0 ? 'text-red-300' : 'text-green-300'}`}>
              {delayMin > 0 ? '+' : ''}{delayMin}m
            </span>
          )}
        </div>
      )}
    </div>
  );
}
