/**
 * data.go.jp CKAN catalog — federated Japan government open-data index.
 * Emits the top N packages as intel items, plus a portal summary.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'data-go-jp-ckan';
const API_URL = 'https://www.data.go.jp/data/api/action/package_search?rows=50';
const TIMEOUT_MS = 10000;

export default async function collectDataGoJpCkan() {
  const data = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS });
  const totalCount = data?.result?.count ?? 0;
  const results = Array.isArray(data?.result?.results) ? data.result.results : [];
  const items = results.map((p) => ({
    uid: intelUid(SOURCE_ID, p.id, p.name),
    title: p.title || p.name,
    body: p.notes || null,
    summary: (p.notes || '').slice(0, 240) || null,
    link: p.name ? `https://www.data.go.jp/data/dataset/${p.name}` : null,
    author: p.organization?.title || null,
    language: 'ja',
    published_at: p.metadata_modified || p.metadata_created || null,
    tags: ['open-data', ...(Array.isArray(p.tags) ? p.tags.slice(0, 5).map((t) => `tag:${t.name}`) : [])],
    properties: {
      ckan_id: p.id || null,
      ckan_name: p.name || null,
      organization: p.organization?.title || null,
      license_id: p.license_id || null,
      resources_count: Array.isArray(p.resources) ? p.resources.length : 0,
    },
  }));

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'Federated Japan government open-data CKAN catalog',
    extraMeta: { total_packages_in_catalog: totalCount },
  });
}
