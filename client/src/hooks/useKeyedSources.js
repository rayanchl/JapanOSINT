import { useState } from 'react';
import { useApi } from './useApi.js';

/**
 * The keyed-source rows (`requiresKey`) of GET /api/status, loaded ONLY when
 * asked for.
 *
 * Why on demand (2026-11-07): the API-keys page and Settings › Probe consent
 * each downloaded the whole /api/status catalogue — every registered source,
 * tens of MB — on page open, to show a few dozen keyed sources. Neither
 * /api/status nor /api/sources/search can filter to keyed sources or say
 * which env var a source reads (search rows carry no `envVars`), so there is
 * no small request that answers this yet; the server-side filter is reported
 * as missing. Until it exists the download happens when the reader asks for
 * it, and the page says what it costs.
 *
 *   const keyed = useKeyedSources();
 *   keyed.load();            // starts the download
 *   keyed.rows / keyed.all   // requiresKey rows / every row
 */
export function useKeyedSources() {
  const [wanted, setWanted] = useState(false);
  const q = useApi(wanted ? '/api/status' : null, { enabled: wanted, deps: [wanted] });
  const all = Array.isArray(q.data?.apis) ? q.data.apis : [];
  return {
    wanted,
    load: () => { if (wanted) q.reload(); else setWanted(true); },
    all,
    rows: all.filter((r) => r.requiresKey),
    data: q.data,
    setData: q.setData,
    error: q.error,
    loading: wanted && q.loading,
    loaded: wanted && Boolean(q.data),
  };
}

export default useKeyedSources;
