import React, { useEffect, useMemo, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { LuStar, LuMapPin, LuExternalLink, LuTrash2, LuLayoutGrid, LuList, LuImage } from 'react-icons/lu';
import { savedStore } from '../../store/savedStore.js';
import PinToCaseButton from '../cases/CasePickerSheet.jsx';
import { CaseRefType, refLink } from '../../hooks/useCases.js';
import { Page, Card, Pill, Button, Input, Segmented, ConfirmDialog, EmptyState, cx, toast } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';

/**
 * Saved — the iOS `SavedTab`. Bookmarks live in this browser (localStorage,
 * `store/savedStore.js`), exactly as the iOS store lives on the device: a
 * personal shortlist, not a workspace object. Anything here can be pinned
 * into a case, which IS shared.
 */
const KIND_LABEL = {
  feature: 'Map feature', intel_item: 'Intel item', entity: 'Entity', camera: 'Camera', search_run: 'Search run',
};
const IMAGE_OPTIONS = [{ value: 'all', label: 'All' }, { value: 'with', label: 'With image' }, { value: 'without', label: 'No image' }];

export function useSavedItems() {
  const [items, setItems] = useState(() => savedStore.all());
  useEffect(() => savedStore.subscribe(setItems), []);
  return items;
}

function isHttp(u) { return typeof u === 'string' && /^https?:\/\//i.test(u); }

function coordText(it) {
  if (!Number.isFinite(it.lat) || !Number.isFinite(it.lon)) return null;
  return `${it.lat.toFixed(5)}, ${it.lon.toFixed(5)}`;
}

function flyTo(it, navigate) {
  if (!Number.isFinite(it.lat) || !Number.isFinite(it.lon)) return;
  navigate('/');
  // MapView registers the listener on mount; give the route a tick to mount it.
  setTimeout(() => window.dispatchEvent(new CustomEvent('japanosint:flyto', { detail: { lat: it.lat, lon: it.lon, zoom: 15 } })), 150);
}

export default function SavedPage() {
  const navigate = useNavigate();
  const items = useSavedItems();
  const [mode, setMode] = useState(() => { try { return window.localStorage.getItem('osint:saved:mode') || 'list'; } catch { return 'list'; } });
  const [kind, setKind] = useState('all');
  const [image, setImage] = useState('all');
  const [layer, setLayer] = useState('all');
  const [q, setQ] = useState('');
  const [confirmClear, setConfirmClear] = useState(false);

  useEffect(() => { try { window.localStorage.setItem('osint:saved:mode', mode); } catch { /* ignore */ } }, [mode]);

  const kinds = useMemo(() => {
    const counts = new Map();
    for (const it of items) counts.set(it.kind, (counts.get(it.kind) || 0) + 1);
    return [{ value: 'all', label: 'All', count: items.length }, ...[...counts.entries()].map(([k, n]) => ({ value: k, label: KIND_LABEL[k] || k, count: n }))];
  }, [items]);

  const layers = useMemo(() => {
    const s = new Set(items.map((it) => it.layerId).filter(Boolean));
    return [...s].sort();
  }, [items]);

  const filtered = useMemo(() => {
    const needle = q.trim().toLowerCase();
    return items.filter((it) => {
      if (kind !== 'all' && it.kind !== kind) return false;
      if (image === 'with' && !it.imageURL) return false;
      if (image === 'without' && it.imageURL) return false;
      if (layer !== 'all' && it.layerId !== layer) return false;
      if (!needle) return true;
      const hay = [it.displayName, it.layerId, it.refId, it.link, ...Object.values(it.properties || {}).map((v) => (typeof v === 'string' ? v : ''))]
        .filter(Boolean).join(' ').toLowerCase();
      return hay.includes(needle);
    });
  }, [items, kind, image, layer, q]);

  const filtersActive = kind !== 'all' || image !== 'all' || layer !== 'all' || q.trim() !== '';

  return (
    <Page
      wide
      title="Saved"
      subtitle="Bookmarked map features, intel items and entities — kept in this browser. Pin any of them into a case to share it with the workspace."
      actions={(
        <>
          <Segmented value={mode} onChange={setMode} options={[{ value: 'list', label: <LuList size={13} /> }, { value: 'grid', label: <LuLayoutGrid size={13} /> }]} />
          <Button variant="danger" disabled={items.length === 0} onClick={() => setConfirmClear(true)}><LuTrash2 size={13} /> Clear all</Button>
        </>
      )}
    >
      <div className="flex flex-wrap items-center gap-2">
        <div className="overflow-x-auto"><Segmented value={kind} onChange={setKind} options={kinds} /></div>
        <Segmented value={image} onChange={setImage} options={IMAGE_OPTIONS} />
        {layers.length > 1 && (
          <select value={layer} onChange={(e) => setLayer(e.target.value)} className="px-2 py-1 rounded-md bg-osint-bg border border-osint-border text-xs text-osint-text">
            <option value="all">All layers</option>
            {layers.map((l) => <option key={l} value={l}>{l}</option>)}
          </select>
        )}
        <Input className="flex-1 min-w-[160px]" placeholder="Search name, layer, id, properties…" value={q} onChange={(e) => setQ(e.target.value)} />
      </div>

      {items.length === 0 && (
        <EmptyState icon={<LuStar size={22} className="mx-auto" />} title="No saved items yet."
          action={<Link to="/" className="text-xs text-accent hover:underline">Open the map</Link>}>
          Star features from map popups, intel items or entities to see them here.
        </EmptyState>
      )}
      {items.length > 0 && filtered.length === 0 && (
        <EmptyState title="No saved items match the current filters."
          action={filtersActive && <Button size="sm" onClick={() => { setKind('all'); setImage('all'); setLayer('all'); setQ(''); }}>Reset filters</Button>} />
      )}

      {filtered.length > 0 && mode === 'list' && (
        <ul className="space-y-2">
          {filtered.map((it) => <SavedRow key={it.id} item={it} onFly={() => flyTo(it, navigate)} />)}
        </ul>
      )}
      {filtered.length > 0 && mode === 'grid' && (
        <div className="grid grid-cols-2 md:grid-cols-3 xl:grid-cols-4 gap-3">
          {filtered.map((it) => <SavedCard key={it.id} item={it} onFly={() => flyTo(it, navigate)} />)}
        </div>
      )}

      {items.length > 0 && (
        <div className="text-[11px] text-osint-muted font-mono">
          {filtered.length === items.length ? `${items.length} saved` : `showing ${filtered.length} of ${items.length} saved (filtered)`}
        </div>
      )}

      <ConfirmDialog
        open={confirmClear}
        onClose={() => setConfirmClear(false)}
        onConfirm={() => { savedStore.clear(); setConfirmClear(false); toast('Saved list cleared'); }}
        title="Clear all saved items?"
        confirmLabel="Clear"
        message={`Removes all ${items.length} bookmarks from this browser. Cases and their pinned findings are not affected.`}
      />
    </Page>
  );
}

function Actions({ item, onFly, compact }) {
  const to = refLink(item.kind, item.refId, null);
  const hasCoord = Number.isFinite(item.lat) && Number.isFinite(item.lon);
  return (
    <div className={cx('flex items-center gap-1', compact ? 'flex-wrap' : 'flex-shrink-0')}>
      {hasCoord && <Button size="sm" onClick={onFly} title="Show on map"><LuMapPin size={12} /> Map</Button>}
      {to && <Link to={to} className="inline-flex items-center gap-1 rounded-md border border-osint-border px-2 py-1 text-[11px] text-osint-muted hover:text-accent hover:border-accent/40">Open</Link>}
      {isHttp(item.link) && (
        <a href={item.link} target="_blank" rel="noreferrer" className="inline-flex items-center gap-1 rounded-md border border-osint-border px-2 py-1 text-[11px] text-osint-muted hover:text-accent hover:border-accent/40" title="Open link">
          <LuExternalLink size={12} />
        </a>
      )}
      <PinToCaseButton refType={item.kind} refId={item.refId} label={item.displayName}>Pin</PinToCaseButton>
      <Button size="sm" variant="ghost" title="Unfavorite" onClick={() => { savedStore.remove(item.kind, item.refId); toast('Removed from Saved'); }}>
        <LuStar size={12} fill="currentColor" className="text-accent" />
      </Button>
    </div>
  );
}

function SavedRow({ item, onFly }) {
  const coords = coordText(item);
  return (
    <li>
      <Card padded={false} className="hover:border-accent/50 transition-colors">
        <div className="flex items-center gap-3 p-2.5">
          <Thumb item={item} className="w-14 h-14 rounded-md flex-shrink-0" />
          <div className="min-w-0 flex-1">
            <div className="text-sm text-osint-text font-medium truncate">{item.displayName}</div>
            <div className="flex flex-wrap items-center gap-1.5 mt-0.5">
              <Pill tone="accent">{KIND_LABEL[item.kind] || CaseRefType.label(item.kind)}</Pill>
              {item.layerId && <Pill>{item.layerId}</Pill>}
              <span className="text-[11px] text-osint-muted font-mono">{coords ? `${coords} · ` : ''}saved {relativeTime(item.savedAt)}</span>
            </div>
          </div>
          <div className="hidden sm:block"><Actions item={item} onFly={onFly} /></div>
        </div>
        <div className="sm:hidden px-2.5 pb-2.5"><Actions item={item} onFly={onFly} compact /></div>
      </Card>
    </li>
  );
}

function SavedCard({ item, onFly }) {
  const coords = coordText(item);
  return (
    <Card padded={false} className="overflow-hidden hover:border-accent/50 transition-colors flex flex-col">
      <Thumb item={item} className="w-full h-28" />
      <div className="p-2.5 space-y-1.5 flex-1 flex flex-col">
        <div className="text-sm text-osint-text font-medium line-clamp-2">{item.displayName}</div>
        <div className="text-[11px] text-osint-muted font-mono truncate">{item.layerId || KIND_LABEL[item.kind] || item.kind}{coords ? ` · ${coords}` : ''}</div>
        <div className="mt-auto"><Actions item={item} onFly={onFly} compact /></div>
      </div>
    </Card>
  );
}

function Thumb({ item, className }) {
  const [broken, setBroken] = useState(false);
  if (isHttp(item.imageURL) && !broken) {
    return <img src={item.imageURL} alt="" loading="lazy" onError={() => setBroken(true)} className={cx('object-cover bg-osint-bg', className)} />;
  }
  return (
    <div className={cx('flex items-center justify-center bg-osint-panel text-osint-muted', className)}>
      {item.kind === 'feature' || item.kind === 'camera' ? <LuMapPin size={18} /> : <LuImage size={18} />}
    </div>
  );
}
