import React from 'react';

/**
 * The Search tab's living background (SearchAuraBackground.swift): a calm
 * top-lit amber fade over the surface. `intensity` 0 = idle breath, 1 = a run
 * is in flight (blooms). CSS only; `prefers-reduced-motion` freezes the
 * breath and keeps the still fade. Never intercepts pointer events.
 */
export default function SearchAura({ intensity = 0 }) {
  const lit = Math.max(0, Math.min(1, Number(intensity) || 0));
  return (
    <div
      aria-hidden="true"
      className="search-aura pointer-events-none absolute inset-0 overflow-hidden"
      style={{ '--aura-boost': lit }}
    >
      <style>{`
        .search-aura::before {
          content: '';
          position: absolute; inset: 0;
          /* base glow 0.22–0.30 (breath) + 0.34 × intensity, like the Swift */
          --aura-glow: calc(0.24 + 0.34 * var(--aura-boost, 0));
          background: linear-gradient(
            to bottom,
            rgb(var(--accent) / var(--aura-glow)) 0%,
            rgb(var(--accent) / calc(var(--aura-glow) * 0.35)) 30%,
            transparent 62%
          );
          animation: search-aura-breath 48s ease-in-out infinite;
          transition: opacity 1.4s ease-in-out;
          opacity: calc(0.75 + 0.25 * var(--aura-boost, 0));
        }
        :root[data-theme='light'] .search-aura::before { opacity: calc(0.35 + 0.15 * var(--aura-boost, 0)); }
        @keyframes search-aura-breath {
          0%, 100% { transform: translateY(0) scaleY(1); }
          50% { transform: translateY(-2%) scaleY(1.06); }
        }
        @media (prefers-reduced-motion: reduce) {
          .search-aura::before { animation: none; }
        }
      `}</style>
    </div>
  );
}
