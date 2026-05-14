import React from 'react';

const STATUS_STYLES = {
  online: 'bg-status-online/20 text-status-online border-status-online/30',
  degraded: 'bg-status-degraded/20 text-status-degraded border-status-degraded/30',
  offline: 'bg-status-offline/20 text-status-offline border-status-offline/30',
  pending: 'bg-gray-600/20 text-gray-400 border-gray-500/30',
  gated: 'bg-gray-700/30 text-gray-400 border-gray-600/30',
};

const TYPE_STYLES = {
  API: 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/30',
  Dataset: 'bg-neon-blue/15 text-neon-blue border-neon-blue/30',
  Scraped: 'bg-neon-orange/15 text-neon-orange border-neon-orange/30',
  'Web Request': 'bg-neon-purple/15 text-neon-purple border-neon-purple/30',
};

export default function StatusBadge({ type, value }) {
  let className = 'inline-flex items-center gap-1.5 px-2 py-0.5 rounded-full text-xs font-medium border ';

  if (type === 'status') {
    const key = (value || '').toLowerCase();
    className += STATUS_STYLES[key] || 'bg-gray-700/50 text-gray-400 border-gray-600/30';
  } else if (type === 'type') {
    className += TYPE_STYLES[value] || 'bg-gray-700/50 text-gray-400 border-gray-600/30';
  } else {
    className += 'bg-gray-700/50 text-gray-400 border-gray-600/30';
  }

  return (
    <span className={className}>
      {type === 'status' && (
        <span className={`status-dot status-${(value || '').toLowerCase()}`} />
      )}
      {value}
    </span>
  );
}
