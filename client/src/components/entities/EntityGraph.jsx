import React from 'react';
import { useNavigate } from 'react-router-dom';
import { entityVisual } from '../../utils/entityVisuals.js';

/**
 * Dependency-free relationship graph: a small fixed-step force-directed
 * layout rendered as SVG (no cytoscape/d3 dependency, matching the iOS
 * Canvas approach). Click a node to pivot to its profile. Falls back to a
 * list above ~120 nodes.
 */
export default function EntityGraph({ graph, rootId }) {
  const navigate = useNavigate();
  const nodes = graph?.nodes || [];
  const edges = graph?.edges || [];
  const W = 760;
  const H = 460;
  const [pos, setPos] = React.useState({});

  React.useEffect(() => {
    if (nodes.length === 0 || nodes.length > 120) return undefined;
    // init on a circle, root centred
    const P = {};
    nodes.forEach((n, i) => {
      if (n.id === rootId) P[n.id] = { x: W / 2, y: H / 2, vx: 0, vy: 0 };
      else {
        const a = (i / nodes.length) * Math.PI * 2;
        P[n.id] = { x: W / 2 + Math.cos(a) * 170, y: H / 2 + Math.sin(a) * 150, vx: 0, vy: 0 };
      }
    });
    let raf;
    let iter = 0;
    const step = () => {
      iter += 1;
      // repulsion
      const ids = Object.keys(P);
      for (let i = 0; i < ids.length; i += 1) {
        for (let j = i + 1; j < ids.length; j += 1) {
          const a = P[ids[i]]; const b = P[ids[j]];
          let dx = a.x - b.x; let dy = a.y - b.y;
          let d2 = dx * dx + dy * dy || 0.01;
          const f = 4200 / d2;
          const d = Math.sqrt(d2);
          dx /= d; dy /= d;
          a.vx += dx * f; a.vy += dy * f;
          b.vx -= dx * f; b.vy -= dy * f;
        }
      }
      // spring on edges
      edges.forEach((e) => {
        const a = P[e.source]; const b = P[e.target];
        if (!a || !b) return;
        const dx = b.x - a.x; const dy = b.y - a.y;
        const d = Math.sqrt(dx * dx + dy * dy) || 0.01;
        const f = (d - 120) * 0.02;
        const ux = dx / d; const uy = dy / d;
        a.vx += ux * f; a.vy += uy * f;
        b.vx -= ux * f; b.vy -= uy * f;
      });
      ids.forEach((id) => {
        const p = P[id];
        if (id === rootId) { p.x = W / 2; p.y = H / 2; p.vx = 0; p.vy = 0; return; }
        p.vx *= 0.82; p.vy *= 0.82;
        p.x = Math.max(24, Math.min(W - 24, p.x + p.vx));
        p.y = Math.max(24, Math.min(H - 24, p.y + p.vy));
      });
      setPos({ ...P });
      if (iter < 160) raf = requestAnimationFrame(step);
    };
    raf = requestAnimationFrame(step);
    return () => cancelAnimationFrame(raf);
  }, [graph, rootId]); // eslint-disable-line react-hooks/exhaustive-deps

  if (nodes.length === 0) {
    return <div className="text-sm text-gray-600 py-8 text-center">No relationships yet.</div>;
  }
  if (nodes.length > 120) {
    return (
      <ul className="text-sm space-y-1">
        {edges.slice(0, 200).map((e, i) => (
          <li key={i} className="text-gray-400">
            {e.source} —<span className="text-gray-600"> {e.relationship} </span>→ {e.target}
          </li>
        ))}
      </ul>
    );
  }

  return (
    <svg viewBox={`0 0 ${W} ${H}`} className="w-full rounded border border-osint-border bg-black/20">
      {edges.map((e, i) => {
        const a = pos[e.source]; const b = pos[e.target];
        if (!a || !b) return null;
        return <line key={i} x1={a.x} y1={a.y} x2={b.x} y2={b.y} stroke="#374151" strokeWidth="1" />;
      })}
      {nodes.map((n) => {
        const p = pos[n.id];
        if (!p) return null;
        const v = entityVisual(n.type);
        const isRoot = n.id === rootId;
        return (
          <g
            key={n.id}
            transform={`translate(${p.x},${p.y})`}
            className="cursor-pointer"
            onClick={() => !isRoot && navigate(`/entities/${encodeURIComponent(String(n.type).toLowerCase())}/${encodeURIComponent(n.id)}`)}
          >
            <circle r={isRoot ? 9 : 6} fill={v.dot} stroke={isRoot ? '#22d3ee' : '#0b0f14'} strokeWidth="2" />
            <text x="0" y="-12" textAnchor="middle" fontSize="10" fill="#cbd5e1">
              {String(n.label || n.value).slice(0, 22)}
            </text>
          </g>
        );
      })}
    </svg>
  );
}
