/**
 * Saved (bookmarked) items — the web counterpart of the iOS `SavedStore`.
 * Local to this browser, like the iOS store is local to the device: saving is
 * a personal bookmark, not a workspace object (that is what Cases are for).
 *
 * Shape (superset of the iOS `SavedItem`):
 *   { id, kind, layerId?, displayName, lat?, lon?, imageURL?, properties, link?, savedAt }
 * `kind` ∈ 'feature' | 'intel_item' | 'entity' | 'camera' | 'search_run'
 * — the server's shared ref_type vocabulary, so a saved item can later be
 * pinned into a case with no translation.
 */
const KEY = 'osint:saved:v1';
const listeners = new Set();
let items = null;

function load() {
  if (items) return items;
  try {
    const raw = window.localStorage.getItem(KEY);
    const parsed = raw ? JSON.parse(raw) : [];
    items = Array.isArray(parsed) ? parsed : [];
  } catch { items = []; }
  return items;
}

function persist() {
  try { window.localStorage.setItem(KEY, JSON.stringify(items)); } catch { /* quota / private mode */ }
  const snap = [...items];
  listeners.forEach((l) => l(snap));
}

export function savedKey(kind, id) { return `${kind}:${id}`; }

export const savedStore = {
  all() { return [...load()]; },
  has(kind, id) { return load().some((x) => x.id === savedKey(kind, id)); },
  get(kind, id) { return load().find((x) => x.id === savedKey(kind, id)) || null; },
  /** Upsert. `item` needs at least { kind, refId, displayName }. */
  save(item) {
    const id = savedKey(item.kind, item.refId ?? item.id);
    const row = {
      id,
      kind: item.kind,
      refId: String(item.refId ?? item.id),
      layerId: item.layerId ?? null,
      displayName: item.displayName || item.title || id,
      lat: Number.isFinite(item.lat) ? item.lat : null,
      lon: Number.isFinite(item.lon) ? item.lon : null,
      imageURL: item.imageURL ?? null,
      link: item.link ?? null,
      properties: item.properties ?? {},
      savedAt: new Date().toISOString(),
    };
    const xs = load();
    const i = xs.findIndex((x) => x.id === id);
    if (i >= 0) xs[i] = { ...xs[i], ...row, savedAt: xs[i].savedAt };
    else xs.unshift(row);
    persist();
    return row;
  },
  remove(kind, id) {
    const key = savedKey(kind, id);
    items = load().filter((x) => x.id !== key);
    persist();
  },
  toggle(item) {
    if (savedStore.has(item.kind, item.refId ?? item.id)) { savedStore.remove(item.kind, item.refId ?? item.id); return false; }
    savedStore.save(item); return true;
  },
  clear() { items = []; persist(); },
  subscribe(fn) {
    listeners.add(fn);
    fn([...load()]);
    return () => listeners.delete(fn);
  },
};

export default savedStore;
