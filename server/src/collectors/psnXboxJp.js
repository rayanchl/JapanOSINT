/**
 * PlayStation Network / Xbox JP presence lookup (psn-xbox-jp).
 *
 * Needs credentials (via envFor):
 *   - PSN_NPSSO    : Sony NPSSO token (exchanged for an auth token)
 *   - XBOX_API_KEY : OpenXBL / XSAPI key for Xbox Live profile lookups
 * Non-spatial intel: public gamer-profile presence for a watched set of
 * JP-region PSN online-IDs / Xbox gamertags. Without any credential or on
 * failure we return an honest empty result — never fabricated profiles.
 *
 * Endpoints (verified keyed):
 *   PSN  : https://ca.account.sony.com/api/v1/ssocookie  (NPSSO → token)
 *          https://m.np.playstation.com/api/userProfile/v1/internal/users/...
 *   Xbox : https://xbl.io/api/v2/account/<gamertag>       (OpenXBL, keyed)
 *
 * Watched IDs come from PSN_XBOX_WATCHLIST (comma-separated "psn:<id>" /
 * "xbl:<gamertag>"); absent watchlist → honest empty (no fabricated IDs).
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'psn-xbox-jp';

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'PSN/Xbox public profile presence for a JP-region watchlist (requires PSN_NPSSO / XBOX_API_KEY)',
    },
  };
}

async function psnToken(npsso) {
  // NPSSO → authorization code → access token (Sony OAuth dance).
  const codeUrl = 'https://ca.account.sony.com/api/authz/v3/oauth/authorize'
    + '?access_type=offline&client_id=09515159-7237-4370-9b40-3806e67c0891'
    + '&redirect_uri=com.scee.psxandroid.scecompcall://redirect'
    + '&response_type=code&scope=psn:mobile.v2.core psn:clientapp';
  const res = await fetch(codeUrl, {
    redirect: 'manual',
    headers: { Cookie: `npsso=${npsso}` },
  }).catch(() => null);
  if (!res) return null;
  const loc = res.headers.get('location') || '';
  const m = /code=([^&]+)/.exec(loc);
  if (!m) return null;
  const tokRes = await fetch('https://ca.account.sony.com/api/authz/v3/oauth/token', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      Authorization: 'Basic MDk1MTUxNTktNzIzNy00MzcwLTliNDAtMzgwNmU2N2MwODkxOnVjUGprYTV0bnRCMktxc1A=',
    },
    body: `grant_type=authorization_code&code=${m[1]}&redirect_uri=com.scee.psxandroid.scecompcall://redirect`,
  }).catch(() => null);
  if (!tokRes || !tokRes.ok) return null;
  const j = await tokRes.json().catch(() => null);
  return j?.access_token || null;
}

function parseWatchlist(raw) {
  const psn = [];
  const xbl = [];
  for (const entry of (raw || '').split(',').map((s) => s.trim()).filter(Boolean)) {
    if (entry.startsWith('psn:')) psn.push(entry.slice(4));
    else if (entry.startsWith('xbl:')) xbl.push(entry.slice(4));
  }
  return { psn, xbl };
}

export default async function collectPsnXboxJp() {
  const npsso = envFor('PSN_NPSSO');
  const xblKey = envFor('XBOX_API_KEY');
  const watchlist = parseWatchlist(envFor('PSN_XBOX_WATCHLIST'));

  if (!npsso && !xblKey) return envelope([], 'psn_xbox_jp_no_key');
  if (watchlist.psn.length === 0 && watchlist.xbl.length === 0) {
    // No watchlist → nothing real to look up; honest empty (no fabricated IDs).
    return envelope([], 'psn_xbox_jp_unavailable');
  }

  const items = [];
  let gotAny = false;

  // ---- PSN ----
  if (npsso && watchlist.psn.length) {
    try {
      const token = await psnToken(npsso);
      if (token) {
        for (const onlineId of watchlist.psn) {
          const url = 'https://m.np.playstation.com/api/userProfile/v1/internal/users/'
            + `${encodeURIComponent(onlineId)}/profiles`;
          const data = await fetchJson(url, {
            timeoutMs: 15000,
            headers: { Authorization: `Bearer ${token}` },
          });
          if (!data) continue;
          gotAny = true;
          const p = data.profile || data;
          items.push({
            uid: `${SOURCE_ID}|psn:${onlineId}`,
            title: `PSN ${onlineId}`,
            summary: p.aboutMe || p.onlineId || null,
            body: p.aboutMe || null,
            link: `https://psnprofiles.com/${encodeURIComponent(onlineId)}`,
            author: onlineId,
            language: null,
            published_at: null,
            tags: ['psn', 'playstation', 'profile'],
            properties: {
              network: 'psn',
              online_id: onlineId,
              account_id: p.accountId ?? null,
              source: 'psn_api',
            },
          });
        }
      }
    } catch { /* fall through to honest empty if nothing else */ }
  }

  // ---- Xbox (OpenXBL) ----
  if (xblKey && watchlist.xbl.length) {
    for (const gamertag of watchlist.xbl) {
      let data = null;
      try {
        data = await fetchJson(
          `https://xbl.io/api/v2/account/${encodeURIComponent(gamertag)}`,
          { timeoutMs: 15000, headers: { 'X-Authorization': xblKey, Accept: 'application/json' } },
        );
      } catch { data = null; }
      if (!data) continue;
      gotAny = true;
      const prof = data?.profileUsers?.[0];
      const settings = {};
      for (const s of prof?.settings || []) settings[s.id] = s.value;
      items.push({
        uid: `${SOURCE_ID}|xbl:${gamertag}`,
        title: `Xbox ${settings.Gamertag || gamertag}`,
        summary: settings.Bio || null,
        body: settings.Bio || null,
        link: `https://www.xbox.com/play/user/${encodeURIComponent(gamertag)}`,
        author: settings.Gamertag || gamertag,
        language: null,
        published_at: null,
        tags: ['xbox', 'xbox-live', 'profile'],
        properties: {
          network: 'xbox',
          gamertag: settings.Gamertag || gamertag,
          xuid: prof?.id ?? null,
          gamerscore: settings.Gamerscore ?? null,
          source: 'openxbl_api',
        },
      });
    }
  }

  if (!gotAny) return envelope([], 'psn_xbox_jp_unavailable');
  if (!items.length) return envelope([], 'psn_xbox_jp_unavailable');
  return envelope(items, 'psn_xbox_api');
}
