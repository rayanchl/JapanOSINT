import { useEffect, useState } from 'react';
import LineChip from './LineChip.jsx';
import apiUrl from '../../../utils/apiUrl.js';

export default function VehiclePopup({ properties }) {
  const tripId = properties?.trip_id;
  const synthetic = properties?.synthetic === true || properties?.synthetic === 'true';
  const [data, setData] = useState(null);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (synthetic || !tripId) return;   // nothing to look up; see the render below
    let cancelled = false;
    (async () => {
      try {
        const r = await fetch(apiUrl(`/api/transit/vehicle/${encodeURIComponent(tripId)}/info`));
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        const body = await r.json();
        if (!cancelled) setData(body);
      } catch (e) {
        if (!cancelled) setError(e.message);
      }
    })();
    return () => { cancelled = true; };
  }, [tripId]);

  // An estimated marker must say so plainly, and must not imply a real vehicle
  // sits here. The old copy for this case was "Not a schedule-backed vehicle",
  // which reads as a lookup failure rather than as "nothing observed this".
  if (synthetic) {
    return (
      <div className="space-y-1">
        <div className="text-xs font-medium text-amber-300">Estimated position</div>
        <div className="text-xs text-gray-400">
          No live or scheduled position is available for this route, so this
          marker is drawn by moving along the route geometry at a constant speed.
          <span className="text-gray-500"> Nothing observed a vehicle here.</span>
        </div>
        {properties?.estimate_basis && (
          <div className="text-[11px] font-mono text-gray-500">{properties.estimate_basis}</div>
        )}
      </div>
    );
  }
  if (!tripId) {
    return <div className="text-xs text-gray-500">No trip id on this vehicle.</div>;
  }
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
