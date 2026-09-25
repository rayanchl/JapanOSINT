/**
 * Guard for rendering externally-sourced strings as an `<a href>` / iframe
 * `src`. React does not sanitize `href` — a `javascript:` URI stored in a
 * record (a scraped camera page, a tweet/toot link, an ingested mention)
 * would execute on click if handed to `href` verbatim. Restricting to
 * http(s) is exactly what MapPopup.jsx's own PropertyTable already does for
 * generic property links; this is the same check, shared so every other
 * "open the source" link gets it too.
 */
export function isSafeUrl(v) {
  return typeof v === 'string' && /^https?:\/\//i.test(v);
}

export default isSafeUrl;
