/**
 * GitLab + Bitbucket public code search — JP-domain secret hunting.
 * https://gitlab.com/api/v4/search
 * https://api.bitbucket.org/2.0/repositories
 *
 * Companion to `githubLeaksJp` (which only covers GitHub). Many JP firms
 * publish to GitLab and Bitbucket; same .env / API-key leak patterns
 * apply.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'gitlab-bitbucket-leaks';
const PROBES = [
  ['gitlab',    'https://gitlab.com/'],
  ['bitbucket', 'https://bitbucket.org/'],
];

export default async function collectGitlabBitbucketLeaks() {
  const hasGitlab = !!process.env.GITLAB_TOKEN;
  const hasBitbucket = !!process.env.BITBUCKET_TOKEN;
  const items = [];
  let anyLive = false;
  for (const [op, url] of PROBES) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, op),
      title: `${op} public code search`,
      summary: (op === 'gitlab' ? hasGitlab : hasBitbucket)
        ? 'Configured' : `Set ${op.toUpperCase()}_TOKEN to enable code search`,
      link: url,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['leak', 'code-search', op, live ? 'reachable' : 'unreachable'],
      properties: { reachable: live, requires_key: true, has_key: op === 'gitlab' ? hasGitlab : hasBitbucket },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'GitLab + Bitbucket public code search — JP-domain secret hunting',
  });
}
