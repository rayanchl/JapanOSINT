import React, { useEffect, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { LuRefreshCw, LuMapPin, LuPencil, LuTrash2, LuBellRing, LuPenTool } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Card, Pill, Button, Input, TextArea, Field, Segmented, Sheet, ConfirmDialog, ErrorNotice, EmptyState,
  LoadingState, BoundNote, CopyButton, toast, cx,
} from '../ui/kit.jsx';

/**
 * Areas of interest — port of iOS `AOIListView`.
 *   GET /api/aoi?limit&cursor → {data:[{id,name,kind,geometry,bbox:[w,s,e,n],created_at}], page:{next_cursor}}
 *   POST /api/aoi {name, kind: bbox|polygon|circle, geometry}
 *     bbox     → [w, s, e, n]
 *     circle   → {lat, lon, radius_m}
 *     polygon  → ring of [lon, lat]
 *   PATCH /api/aoi/:id {name} (merge; shape untouched) · DELETE (409 while a rule references it)
 * Drawing happens on the map (`/?aoi=new`); this page creates from text.
 */
export default function AOIPage() {
  const navigate = useNavigate();
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const [creating, setCreating] = useState(false);
  const [renaming, setRenaming] = useState(null);
  const [confirmDel, setConfirmDel] = useState(null);
  const [busy, setBusy] = useState(null);

  const load = async (cursor) => {
    setLoading(true); setError(null);
    try {
      const j = await api.get('/api/aoi', { query: { limit: 100, cursor } });
      const d = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => (cursor ? [...xs, ...d] : d));
      setPage(j?.page || null);
    } catch (e) { setError(e); }
    finally { setLoading(false); }
  };
  useEffect(() => { load(); }, []);

  const del = async () => {
    const a = confirmDel; setConfirmDel(null); setBusy(a.id);
    try {
      await api.del(`/api/aoi/${encodeURIComponent(a.id)}`);
      toast('Area deleted', { tone: 'accent' });
      setRows((xs) => xs.filter((x) => x.id !== a.id));
    } catch (e) {
      toast(e?.status === 409 ? 'An alert rule still references this area — re-point or delete the rule first.' : errorMessage(e), { tone: 'danger', ttl: 7000 });
    } finally { setBusy(null); }
  };

  return (
    <Page
      title="Areas of interest"
      subtitle="Saved geofences. One shape can back several alert rules; rules resolve it at match time."
      actions={(
        <>
          <Button onClick={() => load()} title="Refresh areas"><LuRefreshCw size={13} /></Button>
          <Button onClick={() => navigate('/?aoi=new')} title="Draw a new area on the map"><LuPenTool size={13} /> Draw on map</Button>
          <Button variant="primary" onClick={() => setCreating(true)}><LuMapPin size={13} /> New area</Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load areas" onRetry={() => load()} />}
      {loading && rows.length === 0 && !error && <LoadingState label="Loading areas…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title="No saved areas" action={<div className="flex gap-2 justify-center"><Button onClick={() => navigate('/?aoi=new')}>Draw an area</Button><Button variant="primary" onClick={() => setCreating(true)}>Enter coordinates</Button></div>}>
          Draw a polygon, circle or box on the map, or paste GeoJSON here. Then reference it from an alert rule.
        </EmptyState>
      )}

      {rows.length > 0 && (
        <div className="space-y-1.5">
          {rows.map((a) => (
            <Card key={a.id} padded={false}>
              <div className="p-3 flex items-start gap-3 flex-wrap">
                <div className="min-w-0 flex-1">
                  <div className="flex items-center gap-2 flex-wrap">
                    <span className="text-sm font-medium text-osint-text truncate">{a.name}</span>
                    <Pill tone="cyan">{a.kind}</Pill>
                  </div>
                  <div className="text-[11px] text-osint-muted font-mono mt-0.5">{geometrySummary(a)}</div>
                  <div className="text-[11px] text-osint-muted mt-0.5" title={fmtAbs(a.created_at)}>Added {relativeTime(a.created_at)} · <span className="font-mono">{a.id}</span></div>
                </div>
                <div className="flex items-center gap-1 flex-wrap">
                  <Button size="sm" onClick={() => navigate(`/?aoi=${encodeURIComponent(a.id)}`)} title="Show on map"><LuMapPin size={12} /> Map</Button>
                  <Link to={`/console/alerts?aoi=${encodeURIComponent(a.id)}`}><Button size="sm" title="Alert on this area"><LuBellRing size={12} /> Alert</Button></Link>
                  <Button size="sm" onClick={() => setRenaming(a)} title="Rename"><LuPencil size={12} /></Button>
                  <CopyButton text={a.id} label="Copy id" />
                  <Button size="sm" variant="ghost" busy={busy === a.id} onClick={() => setConfirmDel(a)} title="Delete"><LuTrash2 size={12} /></Button>
                </div>
              </div>
            </Card>
          ))}
          <div className="flex items-center justify-between">
            <BoundNote shown={rows.length} total={page?.next_cursor ? rows.length + 1 : rows.length} noun="areas" />
            {page?.next_cursor && <Button size="sm" busy={loading} onClick={() => load(page.next_cursor)}>Load more</Button>}
          </div>
        </div>
      )}

      <CreateAoiSheet open={creating} onClose={() => setCreating(false)} onCreated={(row) => { setCreating(false); setRows((xs) => [row, ...xs]); }} />

      <RenameSheet aoi={renaming} onClose={() => setRenaming(null)} onRenamed={(row) => { setRenaming(null); setRows((xs) => xs.map((x) => (x.id === row.id ? row : x))); }} />

      <ConfirmDialog
        open={confirmDel != null}
        onClose={() => setConfirmDel(null)}
        onConfirm={del}
        title={`Delete “${confirmDel?.name || ''}”?`}
        confirmLabel="Delete"
        message="The server refuses (409) while an alert rule still references this area."
      />
    </Page>
  );
}

function geometrySummary(a) {
  const g = a.geometry;
  if (a.kind === 'circle' && g && typeof g === 'object') return `centre ${Number(g.lat).toFixed(4)}, ${Number(g.lon).toFixed(4)} · radius ${Math.round(g.radius_m)} m`;
  if (a.kind === 'polygon' && Array.isArray(g)) return `${g.length} vertices${Array.isArray(a.bbox) ? ` · bbox ${a.bbox.map((n) => Number(n).toFixed(3)).join(', ')}` : ''}`;
  if (Array.isArray(a.bbox)) return `bbox ${a.bbox.map((n) => Number(n).toFixed(4)).join(', ')}`;
  if (Array.isArray(g)) return `bbox ${g.map((n) => Number(n).toFixed(4)).join(', ')}`;
  return 'geometry not reported';
}

/** Parse pasted GeoJSON (Feature / FeatureCollection / Geometry) or a bare ring into a polygon ring. */
function ringFromGeoJSON(text) {
  let j;
  try { j = JSON.parse(text); } catch { throw new Error('Not valid JSON.'); }
  const geom = j?.type === 'FeatureCollection' ? j.features?.[0]?.geometry
    : j?.type === 'Feature' ? j.geometry : j;
  if (Array.isArray(geom)) {
    if (geom.length >= 3 && geom.every((p) => Array.isArray(p) && p.length >= 2)) return geom;
    throw new Error('A bare ring needs at least three [lon, lat] points.');
  }
  if (geom?.type === 'Polygon' && Array.isArray(geom.coordinates?.[0])) return geom.coordinates[0];
  if (geom?.type === 'MultiPolygon' && Array.isArray(geom.coordinates?.[0]?.[0])) return geom.coordinates[0][0];
  throw new Error('Expected a GeoJSON Polygon (or a Feature wrapping one).');
}

function CreateAoiSheet({ open, onClose, onCreated }) {
  const [name, setName] = useState('');
  const [kind, setKind] = useState('polygon');
  const [geo, setGeo] = useState('');
  const [bbox, setBbox] = useState('');
  const [circle, setCircle] = useState({ lat: '', lon: '', radius_m: '1000' });
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  useEffect(() => { if (open) { setError(null); } }, [open]);

  const save = async () => {
    setError(null);
    try {
      if (!name.trim()) throw new Error('Name is required.');
      let geometry;
      if (kind === 'polygon') geometry = ringFromGeoJSON(geo);
      else if (kind === 'bbox') {
        geometry = bbox.split(',').map((s) => Number(s.trim()));
        if (geometry.length !== 4 || geometry.some((n) => !Number.isFinite(n))) throw new Error('bbox must be four numbers: w, s, e, n');
      } else {
        geometry = { lat: Number(circle.lat), lon: Number(circle.lon), radius_m: Number(circle.radius_m) };
        if (!Number.isFinite(geometry.lat) || !Number.isFinite(geometry.lon) || !(geometry.radius_m > 0)) throw new Error('Circle needs lat, lon and a positive radius in metres.');
      }
      setSaving(true);
      const r = await api.post('/api/aoi', { name: name.trim(), kind, geometry });
      toast('Area saved', { tone: 'accent' });
      setName(''); setGeo(''); setBbox('');
      onCreated(r?.data ?? r);
    } catch (e) { setError(e); }
    finally { setSaving(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="New area of interest" width="max-w-lg"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} onClick={save}>Save</Button></>}
    >
      <div className="space-y-3">
        <Field label="Name"><Input value={name} onChange={(e) => setName(e.target.value)} placeholder="Name" /></Field>
        <div className="flex items-center justify-between gap-2">
          <span className="text-xs text-osint-muted">Shape</span>
          <Segmented value={kind} onChange={setKind} options={[{ value: 'polygon', label: 'Polygon (GeoJSON)' }, { value: 'circle', label: 'Centre + radius' }, { value: 'bbox', label: 'bbox' }]} />
        </div>
        {kind === 'polygon' && (
          <Field label="GeoJSON Polygon / Feature, or a bare ring of [lon, lat]">
            <TextArea mono rows={7} value={geo} onChange={(e) => setGeo(e.target.value)} placeholder='{"type":"Polygon","coordinates":[[[139.6,35.6],[139.9,35.6],[139.9,35.8],[139.6,35.6]]]}' />
          </Field>
        )}
        {kind === 'bbox' && <Field label="w, s, e, n"><Input mono value={bbox} onChange={(e) => setBbox(e.target.value)} placeholder="139.5, 35.5, 140.0, 35.9" /></Field>}
        {kind === 'circle' && (
          <div className="grid grid-cols-3 gap-2">
            <Field label="lat"><Input mono value={circle.lat} onChange={(e) => setCircle({ ...circle, lat: e.target.value })} placeholder="35.68" /></Field>
            <Field label="lon"><Input mono value={circle.lon} onChange={(e) => setCircle({ ...circle, lon: e.target.value })} placeholder="139.76" /></Field>
            <Field label="radius (m)"><Input mono value={circle.radius_m} onChange={(e) => setCircle({ ...circle, radius_m: e.target.value })} /></Field>
          </div>
        )}
        <div className="text-[11px] text-osint-muted">Prefer drawing? <Link to="/?aoi=new" className="text-accent">Draw on the map</Link> instead.</div>
        {error && <ErrorNotice error={error} title="Could not save area" />}
      </div>
    </Sheet>
  );
}

function RenameSheet({ aoi, onClose, onRenamed }) {
  const [name, setName] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  useEffect(() => { if (aoi) { setName(aoi.name || ''); setError(null); } }, [aoi]);
  const save = async () => {
    setSaving(true); setError(null);
    try {
      const r = await api.patch(`/api/aoi/${encodeURIComponent(aoi.id)}`, { name: name.trim() });
      toast('Area renamed', { tone: 'accent' });
      onRenamed(r?.data ?? { ...aoi, name: name.trim() });
    } catch (e) { setError(e); }
    finally { setSaving(false); }
  };
  return (
    <Sheet open={aoi != null} onClose={onClose} title="Rename area" width="max-w-sm"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} disabled={!name.trim()} onClick={save}>Save</Button></>}
    >
      <div className="space-y-2">
        <Field label="Name" hint="The shape is unchanged — only the name every rule shows for it."><Input value={name} onChange={(e) => setName(e.target.value)} /></Field>
        {error && <ErrorNotice error={error} title="Rename failed" />}
      </div>
    </Sheet>
  );
}
