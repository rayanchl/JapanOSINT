import LineChip from './LineChip.jsx';

function fmtMinutes(wallSec, nowSec) {
  const raw = wallSec - nowSec;
  const mins = Math.round(raw / 60);
  if (mins < 1) return 'now';
  if (mins < 60) return `${mins}m`;
  return `${Math.floor(mins / 60)}h${mins % 60}m`;
}

function fmtDelay(sec) {
  if (sec == null) return null;
  if (Math.abs(sec) < 30) return null; // under 30s → on time
  const mins = Math.round(sec / 60);
  return mins > 0 ? `+${mins}m` : `${mins}m`;
}

export default function DeparturesBoard({ title, rows, emptyMsg, nowSec }) {
  if (!rows || rows.length === 0) {
    return <div className="py-1 text-xs text-gray-500">{emptyMsg}</div>;
  }
  return (
    <div>
      <div className="text-[10px] uppercase tracking-wider text-gray-500 mb-1">{title}</div>
      <div className="space-y-0.5">
        {rows.map((d, i) => {
          const delay = fmtDelay(d.delay_s);
          return (
            <div key={`${d.trip_id}-${i}`} className="flex items-center gap-1.5 text-xs">
              <LineChip
                color={d.route_color ? `#${d.route_color}` : null}
                ref={d.route_short}
                name={d.route_long}
              />
              <span className="flex-1 truncate text-gray-300" title={d.headsign || ''}>
                {d.headsign || ''}
              </span>
              <span className="font-mono text-gray-200 tabular-nums">
                {fmtMinutes(d.wall_sec, nowSec)}
              </span>
              {delay && (
                <span
                  className={`font-mono text-[10px] tabular-nums px-1 rounded ${
                    d.delay_s > 0 ? 'text-red-300 bg-red-900/40' : 'text-green-300 bg-green-900/40'
                  }`}
                >
                  {delay}
                </span>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
