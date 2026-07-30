import React from 'react';
import { typeLabel } from '../../utils/normalizeSource.js';

const STATUS_STYLES = {
  online: 'bg-status-online/20 text-status-online border-status-online/30',
  degraded: 'bg-status-degraded/20 text-status-degraded border-status-degraded/30',
  offline: 'bg-status-offline/20 text-status-offline border-status-offline/30',
  pending: 'bg-gray-600/20 text-gray-400 border-gray-500/30',
  gated: 'bg-gray-700/30 text-gray-400 border-gray-600/30',
};

// Keyed on the wire values — schema.sql constrains `type` to lowercase
// 'api' | 'dataset' | 'scraped' | 'web_request'. The display label is
// derived separately via typeLabel().
const TYPE_STYLES = {
  api: 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/30',
  dataset: 'bg-neon-blue/15 text-neon-blue border-neon-blue/30',
  scraped: 'bg-neon-orange/15 text-neon-orange border-neon-orange/30',
  web_request: 'bg-neon-purple/15 text-neon-purple border-neon-purple/30',
};

export default function StatusBadge({ type, value }) {
  let className = 'inline-flex items-center gap-1.5 px-2 py-0.5 rounded-full text-xs font-medium border ';
  let label = value;

  if (type === 'status') {
    const key = (value || '').toLowerCase();
    className += STATUS_STYLES[key] || 'bg-gray-700/50 text-gray-400 border-gray-600/30';
  } else if (type === 'type') {
    const key = (value || '').toLowerCase();
    className += TYPE_STYLES[key] || 'bg-gray-700/50 text-gray-400 border-gray-600/30';
    label = typeLabel(value);
  } else {
    className += 'bg-gray-700/50 text-gray-400 border-gray-600/30';
  }

  return (
    <span className={className}>
      {type === 'status' && (
        <span className={`status-dot status-${(value || '').toLowerCase()}`} />
      )}
      {label}
    </span>
  );
}
