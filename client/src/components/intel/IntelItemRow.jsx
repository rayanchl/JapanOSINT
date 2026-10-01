import React from 'react';
import { useNavigate } from 'react-router-dom';
import { Pill, cx } from '../ui/kit.jsx';
import SaveStarButton from '../saved/SaveStarButton.jsx';
import PinToCaseButton from '../cases/CasePickerSheet.jsx';
import { relativeTime } from '../../utils/time.js';

export function itemHref(uid) { return `/intel/items/${encodeURIComponent(uid)}`; }

/** Which title to show, given the row's translation (if the list was fetched with lang_view). */
export function displayTitle(item, view = 'original') {
  const t = item?.translation;
  if (view !== 'original' && t?.title) return t.title;
  return item?.title || item?._excerpt || item?.snippet || '(untitled)';
}

/** One feed row — the iOS `IntelItemRow`. Click opens /intel/items/:uid;
 *  the row itself travels in router state so the detail page keeps any
 *  translation the list already carried. */
export default function IntelItemRow({ item, showSource = true, view = 'original', dense = false }) {
  const navigate = useNavigate();
  const uid = item.uid;
  const when = item.published_at || item.fetched_at;
  const t = item.translation;
  const sem = item.semantic;
  const score = item.score;
  return (
    <div
      role="link"
      tabIndex={0}
      onClick={() => navigate(itemHref(uid), { state: { item } })}
      onKeyDown={(e) => { if (e.key === 'Enter') navigate(itemHref(uid), { state: { item } }); }}
      className={cx('rounded-[10px] border border-osint-border bg-osint-surface hover:border-accent/50 cursor-pointer transition-colors', dense ? 'px-3 py-2' : 'p-3')}
    >
      <div className="flex items-start gap-2">
        <div className="min-w-0 flex-1">
          <div className="flex flex-wrap items-center gap-1.5 text-[10px] text-osint-muted font-mono mb-0.5">
            {showSource && <span className="text-accent truncate max-w-[220px]">{item.provenance?.source_name || item.source_id}</span>}
            {item.record_type && <Pill>{item.record_type}</Pill>}
            {item.language && <Pill>{item.language}</Pill>}
            {when && <span title={when}>{relativeTime(when)}</span>}
            {item.lat != null && item.lon != null && <span title="geolocated">◎ {Number(item.lat).toFixed(3)}, {Number(item.lon).toFixed(3)}</span>}
            {item.cluster_id && <Pill tone="cyan" title={`near-duplicate cluster ${item.cluster_id}`}>cluster</Pill>}
            {sem && <Pill tone="purple" title={`semantic rank ${sem.rank}, score ${sem.score}`}>sem #{sem.rank}</Pill>}
            {score?.score != null && <Pill tone="accent" title={`bm25 ${score.bm25} · trust ${score.trust} · decay ${score.decay}`}>score {Number(score.score).toFixed(2)}</Pill>}
          </div>
          <div className="text-sm text-osint-text leading-snug">{displayTitle(item, view)}</div>
          {view === 'both' && t?.title && item.title && t.title !== item.title && (
            <div className="text-xs text-osint-muted italic">{item.title}</div>
          )}
          {(item.summary || item.snippet || item._excerpt) && !dense && (
            <div className="text-xs text-osint-muted mt-0.5 line-clamp-2">
              {view !== 'original' && t?.summary ? t.summary : (item.snippet || item.summary || item._excerpt)}
            </div>
          )}
          {t?.machine && view !== 'original' && (
            <div className="text-[10px] text-accent/80 mt-0.5">machine translation ({t.engine || 'engine unknown'})</div>
          )}
          {Array.isArray(item.tags) && item.tags.length > 0 && !dense && (
            <div className="flex flex-wrap gap-1 mt-1">
              {item.tags.slice(0, 6).map((tg) => <Pill key={tg}>{tg}</Pill>)}
              {item.tags.length > 6 && <span className="text-[10px] text-osint-muted font-mono">+{item.tags.length - 6} tags</span>}
            </div>
          )}
        </div>
        <div className="flex items-center gap-1 flex-shrink-0" onClick={(e) => e.stopPropagation()} onKeyDown={(e) => e.stopPropagation()}>
          <SaveStarButton size="sm" item={{ kind: 'intel_item', refId: uid, displayName: displayTitle(item), link: item.link, lat: item.lat ?? undefined, lon: item.lon ?? undefined }} />
          <PinToCaseButton refType="intel_item" refId={uid} label={displayTitle(item)} size="sm">{''}</PinToCaseButton>
        </div>
      </div>
    </div>
  );
}
