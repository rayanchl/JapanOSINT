/**
 * Lazy headless-browser screenshot utility.
 *
 * Embed-blocked webcams (X-Frame-Options: DENY, CSP frame-ancestors, etc.)
 * can't be iframed in the map popup. Instead, we load the page in a shared
 * Chromium instance, dismiss the cookie banner, wait for the video element /
 * poster to appear, and return a JPEG buffer the /api/cameras/snapshot route
 * can cache + stream.
 *
 * One browser is launched lazily on first call and reused; each capture
 * gets its own BrowserContext so cookies / storage don't leak between
 * sources.
 */

let _browserPromise = null;

// Force Japanese locale on every context so aggregator sites (Skyline,
// webcamera24, etc.) return Japanese pages regardless of the host OS locale.
// Japanese titles geocode far better via GSI + OSM `name:ja` than the French
// transliterations we were getting before.
const JP_LOCALE = 'ja-JP';
const JP_ACCEPT_LANGUAGE = 'ja-JP,ja;q=0.9,en;q=0.5';

// ─── Concurrency gate ───────────────────────────────────────────────────────
// Every helper in this file (captureSnapshot, renderHtml, extractYouTubeEmbed)
// opens its own BrowserContext on the shared Chromium. Without a cap, a burst
// of 50+ callers (happens during a full camera-discovery run) pile contexts on
// the same browser and every page.goto races for the same CPU, triggering
// 15–20 s timeouts. We bound concurrency so slow callers queue instead of
// timing out.
const MAX_CONCURRENT_BROWSER_JOBS = 4;
let _activeJobs = 0;
const _waiters = [];

function acquireSlot() {
  if (_activeJobs < MAX_CONCURRENT_BROWSER_JOBS) {
    _activeJobs += 1;
    return Promise.resolve();
  }
  return new Promise((resolve) => _waiters.push(resolve));
}

function releaseSlot() {
  const next = _waiters.shift();
  if (next) next();
  else _activeJobs = Math.max(0, _activeJobs - 1);
}

async function withBrowserSlot(fn) {
  await acquireSlot();
  try {
    return await fn();
  } finally {
    releaseSlot();
  }
}

async function getBrowser() {
  if (_browserPromise) return _browserPromise;
  _browserPromise = (async () => {
    let chromium;
    try {
      ({ chromium } = await import('playwright'));
    } catch (err) {
      const e = new Error(
        'playwright is not installed. Run `npm install` in server/ (postinstall downloads Chromium).',
      );
      e.cause = err;
      throw e;
    }
    return chromium.launch({
      headless: true,
      args: ['--no-sandbox', '--disable-dev-shm-usage'],
    });
  })();
  try {
    return await _browserPromise;
  } catch (err) {
    _browserPromise = null;
    throw err;
  }
}

// Common "reject all" / "refuse" buttons across GDPR consent frameworks
// (OneTrust, Cookiebot, Didomi, Quantcast, Termly, and homegrown banners).
// Ordered loosely by specificity — selectors first, then text-based fallbacks.
const COOKIE_REJECT_SELECTORS = [
  '#onetrust-reject-all-handler',
  '#onetrust-pc-btn-handler',
  'button#rejectAll',
  'button.cookie-reject',
  '.cc-deny',
  '.cc-dismiss',
  '#didomi-notice-disagree-button',
  'button[aria-label*="Reject" i]',
  'button[aria-label*="Refuse" i]',
  'button[aria-label*="Rifiuta" i]',
  'button[data-testid*="reject" i]',
  'button[data-cy*="reject" i]',
  '.qc-cmp2-summary-buttons button:nth-child(1)',
];

const COOKIE_REJECT_TEXTS = [
  'Reject all', 'Reject All', 'Reject', 'Refuse all', 'Refuse', 'Decline',
  'Deny', 'Disagree', 'Rifiuta tutto', 'Rifiuta', 'Tout refuser', 'Refuser',
  'すべて拒否', '拒否', 'Ablehnen', 'Alle ablehnen', '거부',
];

/**
 * Best-effort cookie banner dismissal. Tries CSS selectors, then iterates
 * through visible buttons looking for reject-style text. Safe to call
 * unconditionally — silent no-op when no banner is present.
 */
async function dismissCookieBanner(page) {
  try {
    for (const sel of COOKIE_REJECT_SELECTORS) {
      const el = await page.$(sel);
      if (el) {
        try { await el.click({ timeout: 1500 }); return true; } catch { /* keep trying */ }
      }
    }
    // Text-based fallback across all buttons and role=button elements.
    const clicked = await page.evaluate((texts) => {
      const norm = (s) => (s || '').trim().toLowerCase();
      const wanted = new Set(texts.map((t) => t.toLowerCase()));
      const candidates = Array.from(
        document.querySelectorAll('button, [role="button"], a.cc-btn, a.cookie-reject'),
      );
      for (const el of candidates) {
        const label = norm(el.innerText || el.textContent || el.getAttribute('aria-label'));
        if (!label) continue;
        if (wanted.has(label)) {
          el.click();
          return true;
        }
        // Partial match for compound labels like "Reject all cookies".
        for (const t of wanted) {
          if (label.startsWith(t)) { el.click(); return true; }
        }
      }
      return false;
    }, COOKIE_REJECT_TEXTS);
    return clicked;
  } catch {
    return false;
  }
}

/**
 * Capture a JPEG screenshot of a webcam page.
 *
 * @param {string} url
 * @param {object} [opts]
 * @param {number} [opts.timeoutMs=20000]
 * @param {number} [opts.settleMs=3000]
 * @param {{width:number,height:number}} [opts.viewport]
 * @returns {Promise<Buffer|null>}
 */
export async function captureSnapshot(url, opts = {}) {
  return withBrowserSlot(() => _captureSnapshot(url, opts));
}

async function _captureSnapshot(url, opts = {}) {
  const {
    timeoutMs = 20000,
    settleMs = 3000,
    viewport = { width: 1280, height: 720 },
  } = opts;

  let context = null;
  let page = null;
  try {
    const browser = await getBrowser();
    context = await browser.newContext({
      viewport,
      userAgent:
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 ' +
        '(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
      ignoreHTTPSErrors: true,
      locale: JP_LOCALE,
      extraHTTPHeaders: { 'Accept-Language': JP_ACCEPT_LANGUAGE },
    });
    page = await context.newPage();
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: timeoutMs });
    // Accept the cookie banner — several sources (SkylineWebcams' Funding
    // Choices, webcamera24's consent wall) only dismiss the overlay on accept,
    // so a reject click leaves the card on top of the video poster.
    await acceptCookieBanner(page);
    await page.waitForTimeout(settleMs);
    // Two-stage banners (primary "Consent" → vendor preferences "Accept all")
    // need a second pass.
    await acceptCookieBanner(page, { waitMs: 1500 });
    await page.waitForTimeout(Math.min(settleMs, 1500));
    const buf = await page.screenshot({
      type: 'jpeg',
      quality: 70,
      fullPage: false,
    });
    return buf;
  } catch (err) {
    console.warn(`[screenshot] capture failed for ${url}: ${err.message}`);
    return null;
  } finally {
    try { if (page) await page.close(); } catch { /* ignore */ }
    try { if (context) await context.close(); } catch { /* ignore */ }
  }
}

/**
 * Fetch raw HTML using the shared Chromium instance. Useful for JS-rendered
 * aggregator listings that return empty bodies to plain fetch().
 */
export async function renderHtml(url, opts = {}) {
  return withBrowserSlot(() => _renderHtml(url, opts));
}

async function _renderHtml(url, opts = {}) {
  const {
    timeoutMs = 20000,
    settleMs = 2500,
    acceptCookies = true,   // default: click "Accept all" (some sites hide content until consent); set false to refuse
    scrollPasses = 0,       // number of page-down scrolls for lazy-loaded content
    userAgent,              // override default UA (e.g. to pass bot filters)
  } = opts;
  let context = null;
  let page = null;
  try {
    const browser = await getBrowser();
    context = await browser.newContext({
      ignoreHTTPSErrors: true,
      locale: JP_LOCALE,
      extraHTTPHeaders: { 'Accept-Language': JP_ACCEPT_LANGUAGE },
      ...(userAgent ? { userAgent } : {}),
    });
    page = await context.newPage();
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: timeoutMs });
    if (acceptCookies) {
      // acceptCookieBanner waits for a selector to appear (up to 5 s), so we
      // don't need a fixed pre-click delay here.
      await acceptCookieBanner(page);
      await page.waitForTimeout(settleMs);
      // Some banners are two-stage (primary "Consent" → secondary "Accept all"
      // in a vendor preferences dialog). Try a second pass with a short wait.
      await acceptCookieBanner(page, { waitMs: 1500 });
    } else {
      await page.waitForTimeout(Math.min(settleMs, 1200));
      await dismissCookieBanner(page);
      await page.waitForTimeout(settleMs);
    }
    for (let i = 0; i < scrollPasses; i++) {
      await page.evaluate(() => window.scrollBy(0, 1500));
      await page.waitForTimeout(500);
    }
    return await page.content();
  } catch (err) {
    console.warn(`[screenshot] renderHtml failed for ${url}: ${err.message}`);
    return null;
  } finally {
    try { if (page) await page.close(); } catch { /* ignore */ }
    try { if (context) await context.close(); } catch { /* ignore */ }
  }
}

// "Accept all" variant for sites that hide content behind consent (SkylineWebcams'
// Funding Choices banner, most TCF-based walls). Falls back to dismiss on failure.
const COOKIE_ACCEPT_SELECTORS = [
  'button.fc-cta-consent',
  'button.fc-data-preferences-accept-all',
  '#onetrust-accept-btn-handler',
  'button[aria-label*="Accept all" i]',
  'button[aria-label*="Accept" i]',
  'button[aria-label*="Accepter" i]',
  'button[aria-label*="Tout accepter" i]',
  'button[aria-label*="Zustimmen" i]',
  '#didomi-notice-agree-button',
  '.cc-allow',
  '.cc-accept',
  'button.accept-cookies',
  'button#acceptAll',
];

const COOKIE_ACCEPT_TEXTS = [
  'Accept all', 'Accept All', 'Accept', 'I agree', 'Agree',
  'Allow all', 'Allow', 'Got it', 'OK',
  'Tout accepter', 'Accepter', "J'accepte",
  'Akzeptieren', 'Alle akzeptieren', 'Zustimmen',
  'Accetta tutto', 'Accetta',
  'Aceptar todo', 'Aceptar',
  'すべて許可', '同意する', '許可',
];

async function acceptCookieBanner(page, { waitMs = 5000 } = {}) {
  // Wait for any accept-looking control to render before clicking — consent
  // banners often inject a few hundred ms after domcontentloaded, so a fixed
  // settle-then-click race would miss them.
  const combined = COOKIE_ACCEPT_SELECTORS.join(', ');
  try {
    await page.waitForSelector(combined, { timeout: waitMs, state: 'visible' });
  } catch { /* no banner found in time — fall through to text fallback */ }
  try {
    for (const sel of COOKIE_ACCEPT_SELECTORS) {
      const el = await page.$(sel);
      if (el) {
        try { await el.click({ timeout: 1500 }); return true; } catch { /* keep trying */ }
      }
    }
    // Text-based fallback: walk visible buttons, click the first whose
    // innerText matches an accept-style label in any supported language.
    const clicked = await page.evaluate((texts) => {
      const lower = texts.map((t) => t.toLowerCase());
      const buttons = Array.from(document.querySelectorAll('button, a[role="button"], input[type="button"], input[type="submit"]'));
      for (const b of buttons) {
        const label = (b.innerText || b.value || '').trim().toLowerCase();
        if (!label) continue;
        if (lower.some((t) => label === t || label.startsWith(t))) {
          b.click();
          return true;
        }
      }
      return false;
    }, COOKIE_ACCEPT_TEXTS);
    if (clicked) return true;
  } catch { /* ignore */ }
  return false;
}

// Memoize YouTube extraction across collector runs (process lifetime).
// Value is either a YouTube video ID string or `null` when we checked and
// found nothing — both are worth caching so repeat runs are fast.
const YT_EMBED_CACHE = new Map();

/**
 * Load a webcam page in Chromium, dismiss the cookie banner, and look for a
 * YouTube video. Returns the 11-char video ID or null.
 */
export async function extractYouTubeEmbed(url, opts = {}) {
  if (!url) return null;
  if (YT_EMBED_CACHE.has(url)) return YT_EMBED_CACHE.get(url);
  return withBrowserSlot(() => _extractYouTubeEmbed(url, opts));
}

async function _extractYouTubeEmbed(url, opts = {}) {
  if (YT_EMBED_CACHE.has(url)) return YT_EMBED_CACHE.get(url);
  const { timeoutMs = 15000, settleMs = 2000 } = opts;
  let context = null;
  let page = null;
  try {
    const browser = await getBrowser();
    context = await browser.newContext({
      ignoreHTTPSErrors: true,
      locale: JP_LOCALE,
      extraHTTPHeaders: { 'Accept-Language': JP_ACCEPT_LANGUAGE },
    });
    page = await context.newPage();
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: timeoutMs });
    await page.waitForTimeout(Math.min(settleMs, 1000));
    await dismissCookieBanner(page);
    await page.waitForTimeout(settleMs);

    const videoId = await page.evaluate(() => {
      const idRe = /(?:youtube(?:-nocookie)?\.com\/(?:embed\/|watch\?[^"']*?v=)|youtu\.be\/)([\w-]{11})/;
      // 1) iframes
      for (const f of document.querySelectorAll('iframe')) {
        const src = f.src || f.getAttribute('data-src') || '';
        const m = src.match(idRe);
        if (m) return m[1];
      }
      // 2) any element with data-video-id / data-youtube-id
      for (const el of document.querySelectorAll('[data-video-id], [data-youtube-id], [data-yt-id]')) {
        const v = el.getAttribute('data-video-id')
          || el.getAttribute('data-youtube-id')
          || el.getAttribute('data-yt-id');
        if (v && /^[\w-]{11}$/.test(v)) return v;
      }
      // 3) raw HTML scan
      const m = document.documentElement.outerHTML.match(idRe);
      return m ? m[1] : null;
    });

    YT_EMBED_CACHE.set(url, videoId || null);
    return videoId || null;
  } catch (err) {
    console.warn(`[screenshot] extractYouTubeEmbed failed for ${url}: ${err.message}`);
    YT_EMBED_CACHE.set(url, null);
    return null;
  } finally {
    try { if (page) await page.close(); } catch { /* ignore */ }
    try { if (context) await context.close(); } catch { /* ignore */ }
  }
}

export async function closeBrowser() {
  if (!_browserPromise) return;
  try {
    const b = await _browserPromise;
    await b.close();
  } catch { /* ignore */ }
  _browserPromise = null;
}
