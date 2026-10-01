/* lib/utf8.h — the one bounded UTF-8 decoder.
 *
 * Four hand-rolled copies of this existed (core/fts.c, core/translate.c,
 * core/station_clusterer.c, core/entitystore.c) and three of them read past
 * the end of the buffer: they trusted the lead byte's declared length and
 * indexed s[1..3] without checking those bytes exist. On an exactly-sized
 * heap string ending in a truncated sequence ("...\xF0") that is a read of up
 * to three bytes past the allocation, and the reported advance of 4 then
 * steps the caller's loop OVER the NUL terminator and off into the heap.
 *
 * It is remotely reachable: POST /api/alerts with {"q":"…\xF0"} lands in
 * alert_eval.c's fts_segment() -> fts_has_japanese() on a heap string sized
 * to the attacker's bytes. entitystore.c's copy was the one written
 * correctly; this is that logic, shared, so there is nothing left to drift.
 */
#ifndef JO_UTF8_H
#define JO_UTF8_H

/* Decode one codepoint from the NUL-terminated string `s`. *adv receives the
 * bytes consumed — ALWAYS >= 1, so no caller can spin, and never more than
 * the sequence actually present, so no caller can step over the terminator.
 *
 * A truncated or malformed sequence yields the raw lead BYTE with *adv = 1:
 * best effort, matching what the copies did on their malformed path, and the
 * only choice that cannot read unallocated memory. Overlong forms and
 * surrogates are decoded as written — every caller here classifies the
 * codepoint by script range, and none of those ranges contains one. */
static inline unsigned utf8_decode(const unsigned char *s, int *adv) {
  unsigned c = s[0], cp, need;
  if (c < 0x80)                { *adv = 1; return c; }
  else if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1Fu; }
  else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0Fu; }
  else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07u; }
  else                         { *adv = 1; return c; }  /* stray continuation */
  /* s[i] is only read once s[i-1] proved to be a continuation byte, and the
   * NUL terminator is not one — so this cannot walk off the allocation. */
  for (unsigned i = 1; i <= need; i++) {
    if ((s[i] & 0xC0) != 0x80) { *adv = 1; return c; }
    cp = (cp << 6) | (s[i] & 0x3Fu);
  }
  *adv = (int)need + 1;
  return cp;
}

#endif
