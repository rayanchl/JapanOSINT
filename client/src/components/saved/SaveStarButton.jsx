import React, { useEffect, useState } from 'react';
import { LuStar } from 'react-icons/lu';
import { savedStore } from '../../store/savedStore.js';
import { cx, toast } from '../ui/kit.jsx';

/**
 * Star / unstar anything that has a ref_type + id. Drop it next to a title:
 *   <SaveStarButton item={{ kind: 'intel_item', refId: uid, displayName: title, link }} />
 */
export default function SaveStarButton({ item, size = 'md', className, showLabel = false }) {
  const refId = item?.refId ?? item?.id;
  const [on, setOn] = useState(() => (item ? savedStore.has(item.kind, refId) : false));
  useEffect(() => savedStore.subscribe(() => setOn(item ? savedStore.has(item.kind, refId) : false)), [item, refId]);
  if (!item || refId == null) return null;
  const px = size === 'sm' ? 'p-1' : 'px-2 py-1';
  return (
    <button
      type="button"
      aria-pressed={on}
      title={on ? 'Remove from Saved' : 'Save'}
      onClick={(e) => {
        e.stopPropagation();
        const now = savedStore.toggle({ ...item, refId });
        toast(now ? 'Saved' : 'Removed from Saved', { tone: now ? 'accent' : 'neutral' });
      }}
      className={cx('inline-flex items-center gap-1 rounded-md border text-xs transition-colors', px,
        on ? 'border-accent/40 text-accent bg-accent/10' : 'border-osint-border text-osint-muted hover:text-accent hover:border-accent/40', className)}
    >
      <LuStar size={size === 'sm' ? 12 : 14} fill={on ? 'currentColor' : 'none'} />
      {showLabel && (on ? 'Saved' : 'Save')}
    </button>
  );
}
