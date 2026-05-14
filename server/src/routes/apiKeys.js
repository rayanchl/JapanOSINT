import { Router } from 'express';
import { getAllKnownVarNames } from '../utils/apiCredentials.js';
import { setKey, getKeyValue, hasOverlay } from '../utils/apiKeysStore.js';

const router = Router();

/// Build the in-memory metadata view for one var. Reads the live state from
/// process.env (which already reflects any overlay merged in at boot or via
/// PUT) plus the overlay file for the `hasOverlay` flag.
function metaFor({ name, role }) {
  const v = process.env[name];
  return {
    name,
    role,
    set: typeof v === 'string' && v.length > 0,
    hasOverlay: hasOverlay(name),
  };
}

// GET /api/keys — list every known var the server consumes, with status.
// Never includes values; safe for any LAN client.
router.get('/', (_req, res) => {
  try {
    const list = getAllKnownVarNames().map(metaFor);
    res.json(list);
  } catch (err) {
    console.error('[apiKeys] list failed:', err.message);
    res.status(500).json({ error: 'Failed to list keys' });
  }
});

// GET /api/keys/:name — return the actual current value. Used by the iOS
// "Reveal" flow (Face-ID gated client-side; no server auth — same model as
// the rest of the API). Rejects unknown var names so a caller can't probe
// arbitrary process.env entries.
router.get('/:name', (req, res) => {
  const { name } = req.params;
  const known = getAllKnownVarNames().some((v) => v.name === name);
  if (!known) return res.status(404).json({ error: 'Unknown key' });
  res.json({ name, value: getKeyValue(name) });
});

// PUT /api/keys/:name — body { value: string }. Empty string ≡ clear the
// overlay (falls back to the original .env-baked value if any). Returns the
// updated metadata.
router.put('/:name', (req, res) => {
  const { name } = req.params;
  const known = getAllKnownVarNames().find((v) => v.name === name);
  if (!known) return res.status(400).json({ error: 'Unknown key' });
  const value = typeof req.body?.value === 'string' ? req.body.value : '';
  try {
    setKey(name, value);
    res.json(metaFor(known));
  } catch (err) {
    console.error('[apiKeys] write failed:', err.message);
    res.status(500).json({ error: 'Failed to write key' });
  }
});

export default router;
