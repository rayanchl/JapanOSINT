/**
 * Per-account data this browser keeps outside the session: bookmarks, the
 * map's recent geocode queries, camera favourites and the in-memory OSINT
 * search runs. None of it is a workspace object, so the server cannot scope
 * it — the client has to. It used to survive sign-out, so the next account to
 * sign in on the same browser saw the previous account's bookmarks, queries
 * and search results.
 *
 *   releaseLocalData()       explicit sign-out: wipe it now.
 *   claimLocalData(userId)   on every session adopt: if the data on this
 *                            browser was left by a DIFFERENT account (an
 *                            expired session is not a sign-out), wipe it
 *                            before the new account can see it.
 */
import { savedStore } from '../store/savedStore.js';
import { resetSearchStore } from '../store/searchStore.js';

export const MAP_RECENT_SEARCHES_KEY = 'osint:map:recent-searches';
export const CAMERA_FAVORITES_KEY = 'japanosint.cameraFavorites';
const OWNER_KEY = 'osint:local-data-owner';

function lsGet(k) { try { return window.localStorage.getItem(k); } catch { return null; } }
function lsSet(k, v) { try { window.localStorage.setItem(k, v); } catch { /* private mode */ } }
function lsDel(k) { try { window.localStorage.removeItem(k); } catch { /* private mode */ } }

export function clearLocalUserData() {
  savedStore.clear();
  lsDel(MAP_RECENT_SEARCHES_KEY);
  lsDel(CAMERA_FAVORITES_KEY);
  resetSearchStore();
}

export function claimLocalData(userId) {
  if (!userId) return;
  const owner = lsGet(OWNER_KEY);
  if (owner && owner !== String(userId)) clearLocalUserData();
  lsSet(OWNER_KEY, String(userId));
}

export function releaseLocalData() {
  clearLocalUserData();
  lsDel(OWNER_KEY);
}
