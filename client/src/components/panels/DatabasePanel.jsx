import React, { useState } from 'react';
import DatabaseExplorerTab from './DatabaseExplorerTab';
import DatabaseSchedulerTab from './DatabaseSchedulerTab';

const TABS = [
  { id: 'tables',    label: 'Tables' },
  { id: 'scheduler', label: 'Scheduler' },
];

export default function DatabasePanel({ onClose }) {
  const [tab, setTab] = useState('tables');

  return (
    <div className="glass-panel flex flex-col w-[720px] max-w-[95vw] max-h-[80vh] shadow-xl">
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 border-b border-osint-border">
        <div className="flex items-center gap-1">
          {TABS.map((t) => (
            <button
              key={t.id}
              type="button"
              onClick={() => setTab(t.id)}
              className={`px-2.5 py-1 rounded text-[11px] font-medium border transition-colors ${
                tab === t.id
                  ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                  : 'bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40'
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>
        <button
          type="button"
          onClick={onClose}
          className="text-gray-500 hover:text-neon-red text-sm px-1.5"
          title="Close"
        >
          ✕
        </button>
      </div>

      {/* Body */}
      <div className="flex-1 overflow-hidden">
        {tab === 'tables' && <DatabaseExplorerTab />}
        {tab === 'scheduler' && <DatabaseSchedulerTab />}
      </div>
    </div>
  );
}
