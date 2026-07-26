/* core/media.h — roadmap 27: the media pipeline. Images attached to intel
 * items are fetched once, preserved as evidence, and mined for the three
 * things an image is worth in an OSINT corpus: WHERE it was taken (EXIF GPS),
 * WHAT text is burned into it (OCR), and WHETHER it is the same picture as
 * something already in the corpus (perceptual hash).
 *
 * ══ WHY THIS EXISTS ═══════════════════════════════════════════════════════
 *
 * The corpus already stores image URLs — 40-odd collectors put `image`,
 * `thumbnail_url`, `photo_url` or an <img src> into properties/body — and does
 * nothing with them. Three consequences, in descending order of cost:
 *
 * 1. GEOLOCATION LEFT ON THE TABLE. idx_intel_items_geom_pending exists
 *    precisely because a large slice of the corpus has `lat IS NULL AND
 *    geom_source IS NULL`; those rows never appear on the map. A JPEG shot on
 *    a phone carries the coordinate in its APP1 segment. Reading it needs no
 *    image decoding at all — it is byte-level TIFF-IFD parsing — so it is the
 *    cheapest geolocation source in the product and the only one that is
 *    ground truth rather than inference. That is the deliverable this module
 *    is built around.
 * 2. TEXT IN IMAGES IS INVISIBLE TO SEARCH. Japanese municipal and police
 *    notices are routinely published as a scanned image with no HTML body.
 *    FTS indexes nothing, so the item is unfindable by its own contents.
 * 3. THE SAME PHOTO IS RE-REPORTED ENDLESSLY. simhash.c collapses re-worded
 *    TEXT; nothing collapses a wire photo that twenty aggregators re-host at
 *    twenty URLs. A perceptual hash is the image-side equivalent.
 *
 * ══ THE HARD CONSTRAINT, STATED UP FRONT ══════════════════════════════════
 *
 * There is no libjpeg, no libpng, no stb_image, no ffmpeg and no tesseract in
 * this toolchain, and CONVENTIONS forbids adding a link dependency. Nothing in
 * this file pretends otherwise. The split that falls out of it:
 *
 *   EXIF, image dimensions and content-type sniffing  → BUILT, pure C, in
 *     process, no dependency. These are HEADER parses. A JPEG's APP1 segment,
 *     a PNG's IHDR chunk and a TIFF's IFD0 are structured byte tables sitting
 *     in front of the compressed pixel data; you never touch a Huffman table
 *     or an inflate stream to read them. This is the majority of the file and
 *     it is complete.
 *
 *   pHash and OCR                                     → PLUGGABLE SEAMS.
 *     Both need PIXELS, and pixels need a decoder we do not have. They are
 *     therefore implemented as external-tool shell-outs that run ONLY when the
 *     tool is present on PATH and degrade to a NULL column when it is not.
 *     Neither is exercised in the environment this was written in — see
 *     "WHAT IS AND IS NOT EXERCISED" at the bottom, which says so explicitly
 *     rather than leaving you to find out in production.
 *
 *     The pHash seam is drawn deliberately narrow: the external tool is asked
 *     for nothing but 1024 bytes of raw 8-bit grayscale (a 32x32 downscale) on
 *     stdout, and the DCT, the median threshold and the 64-bit packing are
 *     done HERE, in this file. So the hash is a property of this codebase, not
 *     of whichever ImageMagick build happens to be installed: two deployments
 *     with different tools still produce comparable hashes, and swapping the
 *     decoder does not silently invalidate every stored fingerprint. Handing
 *     the whole hash off to an external "phash" binary would have been fewer
 *     lines and would have made the column meaningless across machines.
 *
 * ══ THE FIVE STAGES ═══════════════════════════════════════════════════════
 *
 *   discover → fetch+preserve → parse → enrich(back-write) → index
 *
 * DISCOVER  media_discover_batch(): items with media_scanned_at IS NULL are
 *   swept for image URLs in `properties` (recursive walk: any string value
 *   that looks like an image URL, plus any value under an image-ish KEY —
 *   image/img/photo/thumb/picture/preview/poster/avatar/enclosure/screenshot —
 *   because CDN URLs frequently carry no file extension), in `body`
 *   (<img src=…> plus bare URLs with an image extension) and in `link`.
 *   Deduped by UNIQUE(item_uid,url); capped at MEDIA_MAX_PER_ITEM.
 *
 * FETCH     One GET through core/httpclient.h per asset, with a Range header
 *   as a soft size cap (see LIMITS). Bytes are preserved through
 *   evidence.h's content-addressed blob store — NOT a second store. Why that
 *   matters: the blob store already does atomic temp+fsync+rename, SHA-256
 *   dedup (the same wire photo re-hosted at twenty URLs is one file on disk)
 *   and byte-budgeted GC. media_assets rows survive blob eviction exactly as
 *   evidence rows do: the sha256 still proves what the bytes were, and the
 *   derived columns (exif/phash/ocr) are already extracted, so an evicted blob
 *   costs nothing but the ability to re-derive.
 *
 *   Attribution: the capture is filed under the ITEM's source_id and the
 *   item's uid, not under this pod, so the custody row says "this image came
 *   in with that item from that source". To make that the only capture,
 *   media_analyze_batch() calls evidence_scope_end() on entry, which switches
 *   OFF httpclient.c's automatic hook for the rest of this thread's run — see
 *   the comment at that call site.
 *
 * PARSE     media_exif_parse() + media_image_dims() + media_sniff_type(), all
 *   in-process over the fetched buffer. Then, only if the tools exist,
 *   media_phash_file() and media_ocr_file().
 *
 * ENRICH    media_geo_backfill() writes EXIF GPS onto the item with
 *   geom_source='exif' — never over an existing coordinate.
 *
 * INDEX     OCR text is pushed into intel_items_fts.keywords so text burned
 *   into an image becomes searchable. See the FTS section — it is the one
 *   place this module has to coexist with another module in a shared column.
 *
 * ══ EXIF: HOSTILE INPUT, AND HOW IT IS CONTAINED ══════════════════════════
 *
 * These bytes come from the open internet and an EXIF block is a graph of
 * 32-bit file offsets that the file itself supplies. That is a hostile
 * pointer graph, and a naive walk of it is a remote out-of-bounds read. The
 * containment, all of which is enforced in ex_ifd()/ex_val():
 *
 *  - Every offset is bounds-checked as `off <= tlen && n <= tlen - off`, which
 *    cannot overflow, instead of the usual `off + n > tlen`, which can.
 *  - The APP1 segment length is validated against the buffer BEFORE the TIFF
 *    base pointer is formed, so the parser can never address past the segment.
 *  - Entry counts are capped (MEDIA_EXIF_MAX_ENTRIES) AND clamped to the
 *    number of 12-byte entries that physically fit in the remaining segment.
 *  - A value offset below 8 is refused: that region is the TIFF header, and an
 *    offset pointing into it is a marker of a crafted file, not a real camera.
 *  - IFD links (SubIFD, GPS IFD, next-IFD) must point FORWARD and are recorded
 *    in a visited-offset set, so a file whose IFD1 points back at IFD0 — the
 *    classic infinite-recursion EXIF bomb — terminates. Recursion depth is
 *    capped independently (MEDIA_EXIF_MAX_DEPTH) as a second, unrelated bound.
 *  - `count * type_size` is checked for 32-bit overflow before it is used.
 *  - The JPEG marker walk refuses a segment length < 2 (which would not
 *    advance) and caps the number of markers examined.
 *  - ASCII values are truncated to the destination, NUL-terminated, and have
 *    control bytes replaced with spaces — these strings end up in JSON and in
 *    a UI, and a raw 0x1B from a crafted Make tag has no business there.
 *  - Rationals with a zero denominator yield 0, never a division.
 *  - A GPS fix is rejected unless its N/S and E/W reference tags are present
 *    and valid, and unless the result is in range and not exactly (0,0).
 *    A hemisphere guessed wrong is a worse outcome than no coordinate, and
 *    (0,0) is what a camera writes when it has no fix.
 *
 * The contract is absolute: a malformed image returns "no EXIF". There is no
 * input for which this parser reads outside the buffer it was handed.
 *
 * Coverage: JPEG APP1/Exif, bare TIFF (a .tif IS a TIFF block), and PNG eXIf
 * chunks. Both byte orders ('II' and 'MM') throughout — there is no global
 * endianness flag; the order lives in the per-parse context, so the parser is
 * re-entrant and thread-safe. (collectors/sources/exif_extractor.c, the
 * pre-existing on-demand EXIF service, uses a file-scope `g_le` and is not.
 * See RECOMMENDED FOLLOW-UPS.)
 *
 * HEIC/AVIF are NOT parsed. Their EXIF lives inside an ISO-BMFF `meta` box
 * behind an item-location table; that is a real parser, not a header peek, and
 * it is out of scope here. media_sniff_type() still identifies them so they
 * are recorded honestly rather than mis-parsed.
 *
 * ══ THE FTS COLLISION WITH translate.c — READ THIS ════════════════════════
 *
 * intel_items_fts is fts5(uid UNINDEXED, title, body, summary, keywords).
 * FTS5 has no ALTER TABLE ADD COLUMN, and roadmap 29 (core/translate.c)
 * already established the only workable answer: additional searchable text is
 * appended into the existing, otherwise-unused `keywords` column, because it
 * is already tokenized and already covered by every unqualified
 * `intel_items_fts MATCH ?` in the codebase. translate.h says so and adds:
 * "if a keyword enricher ever lands it must co-write this column rather than
 * clobber it." This module is that second writer. Saying it loudly, as
 * instructed:
 *
 *   THE TWO MODULES DO COLLIDE, AND NEITHER FILE MAY BE EDITED. translate.c's
 *   fts_put_en() executes `UPDATE intel_items_fts SET keywords=?` with the
 *   English text ALONE. Run after this module, it erases OCR text from the
 *   index. This module is therefore built as the REPAIRING side of the pair:
 *
 *     (a) It never writes OCR alone. media_fts_compose() reads
 *         intel_items.title_en/summary_en/body_en itself and rebuilds
 *         translate.c's exact payload — same concatenation order, same
 *         fts_segment() call — then appends the segmented OCR text. So a write
 *         from THIS module can never lose English.
 *     (b) media_fts_sync() is a bounded, cursor-driven repair pass over items
 *         that have OCR text. It reads the row's current `keywords` back and
 *         rewrites it only when the OCR payload is absent — so a steady state
 *         costs one indexed read per OCR'd item per cycle and zero writes.
 *
 *   Net behaviour: a translate write transiently drops OCR from the index and
 *   the next repair pass restores it, with English intact throughout. Worst-
 *   case staleness is one full cursor cycle, i.e.
 *   ceil(N_ocr / MEDIA_FTS_BATCH) pod ticks. Nothing is ever permanently lost
 *   and English is never clobbered in either direction.
 *
 *   THE CLEAN FIX, for whoever owns both files later: hoist the compose step
 *   into one shared function that both modules call. Until then the ordering
 *   hazard is real and is documented here rather than hidden.
 *
 * NOTE, to prevent a wrong-column bug: intel_items ALSO has base-table columns
 * `keywords`/`keywords_at`/`keywords_failed` (the keyword enricher that never
 * landed). They are unrelated to intel_items_fts.keywords and this module does
 * not touch them.
 *
 * ══ THE geom_source RE-INGEST HAZARD — ALSO READ THIS ═════════════════════
 *
 * core/intel.c's upsert preserves a derived coordinate across re-ingest for
 * EXACTLY ONE value of geom_source:
 *
 *     lat = CASE WHEN excluded.lat IS NOT NULL THEN excluded.lat
 *                WHEN intel_items.geom_source='llm' THEN intel_items.lat
 *                ELSE excluded.lat END
 *
 * A collector re-emitting an ungeocoded item supplies excluded.lat = NULL, so
 * any geom_source that is not 'llm' falls through to ELSE and is WIPED. An
 * EXIF coordinate written by this module therefore survives only until the
 * next scheduled refresh of that item. Two responses, both applied:
 *
 *  1. RECOMMENDED ONE-WORD FIX (orchestrator, core/intel.c SQL_UPSERT):
 *     change `intel_items.geom_source='llm'` to
 *     `intel_items.geom_source IN ('llm','exif')` in all four CASE arms
 *     (lat, lon, geom_source, geom_at). This is the correct fix — EXIF GPS is
 *     strictly more authoritative than the LLM guess that IS preserved today.
 *  2. SELF-HEALING, so the feature is durable WITHOUT that edit:
 *     media_geo_reapply() re-writes the coordinate from the EXIF already
 *     stored in media_assets.exif_json onto any item that has lost it. It runs
 *     every pod tick, is driven by a partial index, and costs one empty index
 *     scan once the corpus is settled. A wiped coordinate comes back within
 *     one tick.
 *
 * ══ LIMITS AND COSTS, HONESTLY ════════════════════════════════════════════
 *
 * SIZE CAP IS PARTLY POST-HOC. http_request() buffers the whole response into
 * memory and exposes no streaming or size hook, and httpclient.c may not be
 * edited from here. So: a `Range: bytes=0-<cap>` request header is sent, which
 * caps the transfer at every server that honours ranges (effectively all
 * image CDNs) and yields 206; a 206 body is still parsed for EXIF, because
 * EXIF lives at the head of the file, but it is flagged `"truncated":true` in
 * exif_json and is NOT passed to pHash or OCR, which need a complete image.
 * A server that ignores Range returns 200 with the full body, and the cap is
 * then enforced after the fact by discarding the asset. The residual exposure
 * is one oversized buffer in RAM for one asset at a time, bounded by
 * MEDIA_TIMEOUT_MS.
 *   PROPER FIX, one line, orchestrator's call — in httpclient.c do_once():
 *       curl_easy_setopt(e, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)N);
 *   which makes curl abort the transfer at N bytes for every caller.
 *
 * SHELL-OUT COST. One fork+exec per asset per tool. That is expensive per
 * image and irrelevant in aggregate at MEDIA_ANALYZE_BATCH assets per 900s
 * tick. The command line is built here, never from remote input, and the only
 * interpolated value is a filesystem path that is either a hex content hash
 * from the evidence store or an mkstemp() name — both validated against
 * [A-Za-z0-9._/-] before use, so there is no path by which a remote URL or
 * filename reaches /bin/sh.
 *
 * TENANCY. Like core/translate.c, core/entity_enrich.c and core/entity_stats.c,
 * the background passes here are corpus-wide with no tenant predicate: they are
 * maintenance over intel_items, not request handlers, and they only ever write
 * derived data back onto the row they read. media_list_for_item() IS a request
 * path and takes the uid the caller was already authorized for — the same
 * stance and the same caveat as translate_shape_items(): do not repurpose it
 * for uids discovered elsewhere.
 *
 * ══ SCHEMA ════════════════════════════════════════════════════════════════
 *
 * media_migrate() performs EVERY statement below, in dependency order, and is
 * idempotent — wiring that one call is sufficient. The breakdown is given so
 * the orchestrator can also place the safe parts in schema.sql for fresh
 * databases, and so the unsafe parts are unmistakable.
 *
 * (A) SAFE for core/schema.sql (no dependency on any new column):
 *
 *   CREATE TABLE IF NOT EXISTS media_assets (
 *     id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, url TEXT NOT NULL,
 *     sha256 TEXT, bytes INTEGER, content_type TEXT,
 *     phash INTEGER, exif_json TEXT, ocr_text TEXT, ocr_conf REAL,
 *     width INTEGER, height INTEGER,
 *     analyzed_at TEXT, analyze_failed INTEGER NOT NULL DEFAULT 0,
 *     UNIQUE(item_uid, url));
 *   CREATE INDEX IF NOT EXISTS idx_media_assets_item ON media_assets(item_uid);
 *   CREATE INDEX IF NOT EXISTS idx_media_assets_sha ON media_assets(sha256);
 *
 *   -- The pending-work index. Partial, so once the backlog is drained it is
 *   -- EMPTY and a pod tick costs one empty index seek instead of a scan of
 *   -- every image ever analyzed. The literal 3 is repeated verbatim in every
 *   -- pending query (MEDIA_PENDING_WHERE in media.c) for the same reason
 *   -- translate.c repeats its own: SQLite uses a partial index only when the
 *   -- query's WHERE *syntactically* implies the index's, which a bound
 *   -- parameter can never do. That is why the retry cap is a compiled-in
 *   -- constant and NOT an environment variable.
 *   --
 *   -- KEYED ON analyze_failed, NOT ON rowid. SQLite REFUSES to index rowid
 *   -- ("no such column: rowid"), and sqlite3_exec abandons the remainder of a
 *   -- script at the first error — so that one illegal statement would take
 *   -- media_state and the two indexes after it down with it, and the module
 *   -- would then fail at runtime somewhere else entirely. It is also the
 *   -- better sort key: never-attempted assets come before ones that have
 *   -- already failed twice, so a batch is not spent re-poking known-bad URLs.
 *   CREATE INDEX IF NOT EXISTS idx_media_assets_pending
 *     ON media_assets(analyze_failed)
 *     WHERE analyzed_at IS NULL AND analyze_failed < 3;
 *
 *   -- Drives media_geo_reapply(); small, because most assets carry no EXIF.
 *   CREATE INDEX IF NOT EXISTS idx_media_assets_exif
 *     ON media_assets(item_uid) WHERE exif_json IS NOT NULL;
 *
 *   -- Drives the OCR->FTS repair cursor. Also small.
 *   CREATE INDEX IF NOT EXISTS idx_media_assets_ocr
 *     ON media_assets(item_uid) WHERE ocr_text IS NOT NULL;
 *
 *   CREATE TABLE IF NOT EXISTS media_state (
 *     k TEXT PRIMARY KEY,
 *     v TEXT NOT NULL);
 *
 * (B) NEW COLUMN on an existing table — ensure_column(), NEVER an ALTER in
 *     schema.sql (db_open() hands that file to ONE sqlite3_exec() on every
 *     boot; the second boot dies with "duplicate column name", exec abandons
 *     the rest of the script, and the process never starts):
 *
 *   ensure_column(db, "intel_items", "media_scanned_at", "TEXT");
 *
 * (C) An index that REFERENCES that new column, and therefore MUST NOT go into
 *     schema.sql — schema.sql runs BEFORE the boot-migration block, so on an
 *     existing database the column does not exist yet and this CREATE would
 *     brick the boot:
 *
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_media_pending
 *     ON intel_items(fetched_at DESC) WHERE media_scanned_at IS NULL;
 *
 *   COST, stated plainly: on an existing corpus every row matches that
 *   predicate, so this index starts the size of the table and costs one table
 *   scan to build — a one-off boot delay on a large database, identical in
 *   kind to simhash.c's idx_intel_items_simhash_todo. It then SHRINKS as
 *   discovery proceeds and is empty when the sweep completes.
 *
 * ══ WIRING THE ORCHESTRATOR MUST DO ═══════════════════════════════════════
 *
 * 1. core/db.c, boot-migration block, beside the other ensure_column() calls:
 *        #include "media.h"
 *        media_migrate(db);
 *    This is the ONLY required wiring. Every entry point below also calls it
 *    lazily (once per process, mutex-guarded), so a missed call degrades to a
 *    one-time migration on first use rather than SQL errors — wire it anyway,
 *    because doing schema work on the first HTTP request is not where you
 *    want it.
 *
 * 2. Scheduled operation: NONE. media.c registers its own source_def
 *    ("media-analyze", collector "_enrich", 900s) via REGISTER_SOURCE, exactly
 *    like core/translate.c. The Makefile globs the "core" .c files so the
 *    constructor links itself in. The underscore prefix is REQUIRED, not
 *    cosmetic: scheduler.c skips fetch_log_write() and anomaly_detect() for
 *    underscore collectors, and this pod emits zero intel_items (it would log
 *    records=0 forever and trip records_drop against its own baseline) and can
 *    run for minutes on a backlog (tripping duration_outlier). It also
 *    inherits the scheduler's skip-if-running serialisation, so two ticks can
 *    never overlap.
 *
 * 3. core/httpd.c — two OPTIONAL read routes, no new auth machinery:
 *
 *      GET /api/intel/items/:uid/media          (plain auth, tenant-agnostic,
 *                                                same shape as the existing
 *                                                .../evidence route)
 *        char *body = media_list_for_item(g_db, uid, limit);
 *        if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
 *        reply_json(c, 200, body); free(body); return;
 *
 *      GET /api/media/capabilities              (operator/admin — it reports
 *                                                which external tools this
 *                                                host has)
 *        char *body = media_capabilities();
 *        reply_json(c, 200, body); free(body); return;
 *
 * 4. No core/main.c change. No new thread. No new link dependency.
 *
 * ══ RECOMMENDED FOLLOW-UPS (NOT required, NOT done here) ══════════════════
 *
 *  - core/intel.c SQL_UPSERT: `geom_source IN ('llm','exif')` — see the
 *    geom_source hazard above. Without it, media_geo_reapply() is doing work
 *    that a four-character SQL change would make unnecessary.
 *  - collectors/sources/exif_extractor.c should delete its private parser and
 *    call media_exif_parse(). Its version has a file-scope endianness global
 *    (not thread-safe under the scheduler), computes `off + 24 > tlen` on
 *    uint32 (overflowable), forms `t + tlen` pointers that can pass the end of
 *    the buffer, follows the GPS IFD with no cycle or depth guard, and reads
 *    no ExifIFD. It is an on-demand OSINT service, so it is fed URLs typed by
 *    an analyst — a smaller blast radius than this pod's, but the same bug.
 *  - core/httpclient.c: CURLOPT_MAXFILESIZE_LARGE — see LIMITS.
 *
 * ══ ENVIRONMENT ═══════════════════════════════════════════════════════════
 *   MEDIA_ENABLED=false        kill switch; anything else = on
 *   MEDIA_DISCOVER_BATCH       items swept for image URLs per tick   (200)
 *   MEDIA_ANALYZE_BATCH        assets fetched+analyzed per tick       (25)
 *   MEDIA_FTS_BATCH            OCR->FTS repair rows examined per tick (500)
 *   MEDIA_GEO_BATCH            geo re-apply rows per tick            (200)
 *   MEDIA_MAX_PER_ITEM         image URLs recorded per item            (8)
 *   MEDIA_MAX_BYTES            per-image byte cap             (8388608 = 8 MiB)
 *   MEDIA_TIMEOUT_MS           per-image fetch timeout               (15000)
 *   MEDIA_PHASH_CMD            override; must emit 1024 raw grayscale bytes
 *                              for a 32x32 image on stdout. "{}" in the string
 *                              is replaced by the file path; if absent, the
 *                              path is appended. Unset → probe PATH for
 *                              magick / convert / gm.
 *   MEDIA_OCR_CMD              override; output is taken as PLAIN TEXT and
 *                              ocr_conf is left NULL (an external tool's
 *                              confidence scale is not ours to invent). "{}"
 *                              substitution as above. Unset → probe PATH for
 *                              tesseract and use its TSV mode, which yields a
 *                              real per-word confidence.
 *   MEDIA_OCR_LANG             tesseract -l value                  (jpn+eng)
 *   MEDIA_OCR_MAX_BYTES        OCR text stored per image             (65536)
 *   MEDIA_NO_TOOLS             set → never shell out; EXIF only
 *
 * ══ WHAT IS AND IS NOT EXERCISED IN THIS ENVIRONMENT ══════════════════════
 *
 * EXERCISED: the EXIF parser, dimension parsing, type sniffing, the DCT and
 * bit-packing half of pHash, and the SQL. The parser was driven against
 * hand-built JPEG/TIFF byte fixtures in both byte orders and against a
 * corpus of deliberately malformed inputs (see the test note in media.c).
 *
 * NOT EXERCISED: the OCR and pHash SHELL-OUTS end to end, because no
 * tesseract and no ImageMagick exist here. What that means concretely is that
 * the tool-probe / command-construction / stdout-parse path has been written
 * and syntax-checked but never run against a real tool. The failure mode is
 * contained by construction — every one of those functions returns 0 and
 * leaves the column NULL on any error, and the pod treats "no tool" and "tool
 * failed" identically — but nobody should read this comment as a claim that
 * pHash works on this host. It does not, because it cannot: there is nothing
 * here that can turn a JPEG into pixels.
 */
#ifndef JO_MEDIA_H
#define JO_MEDIA_H

#include "db.h"
#include "httpclient.h"
#include <stddef.h>
#include <stdint.h>

/* ── parser bounds (see "HOSTILE INPUT" above) ───────────────────────────── */

/* Directory entries honoured in one IFD. Real files carry well under 100;
 * this is a latency/absurdity bound, not a correctness one — entries beyond
 * it are dropped and `truncated` is set. */
#define MEDIA_EXIF_MAX_ENTRIES 512

/* Distinct IFDs visited per image, and the recursion depth allowed. Two
 * independent bounds on purpose: the visited-offset set alone already breaks
 * cycles, and the depth cap alone already bounds the stack; either failing
 * open still leaves the other. */
#define MEDIA_EXIF_MAX_IFDS 16
#define MEDIA_EXIF_MAX_DEPTH 4

/* JPEG segment markers examined before giving up looking for APP1. */
#define MEDIA_JPEG_MAX_MARKERS 4096

/* Consecutive failures after which an asset is abandoned. Compiled in, not an
 * env knob — it is baked into idx_media_assets_pending's WHERE clause and a
 * bound parameter would defeat the partial index. */
#define MEDIA_MAX_FAILURES 3

/* ── EXIF ────────────────────────────────────────────────────────────────── */

/* Everything media_exif_parse() extracts. Fixed-size, no allocation, safe to
 * put on the stack. Absent fields are 0 / "" — never uninitialised: the struct
 * is memset on entry, including on the failure paths. */
typedef struct {
  int  found;            /* 1 = an Exif TIFF block was located and walked    */
  int  little_endian;    /* byte order of that block ('II' → 1)              */
  int  ifds_parsed;      /* IFDs actually visited (diagnostic)               */
  int  entries_parsed;   /* directory entries examined (diagnostic)          */
  int  truncated;        /* a count/offset had to be clamped → file is bad
                          * or the buffer was a partial (206) fetch          */

  int    have_gps;       /* 1 only when ref tags were present AND valid AND
                          * the result is in range and not exactly (0,0)     */
  double lat, lon;
  int    have_alt;
  double altitude_m;     /* negative when GPSAltitudeRef says below sea level */

  int  orientation;      /* 0 = absent, else the EXIF 1..8 value             */
  long width, height;    /* pixels; 0 when absent. Prefer media_image_dims() —
                          * it reads the actual compressed frame header,
                          * whereas these come from tags an editor can lie in */

  char make[96], model[96], software[96], lens[96];
  char datetime_original[32];   /* 0x9003 — when the shutter fired          */
  char datetime_digitized[32];  /* 0x9004                                   */
  char datetime[32];            /* 0x0132 — last modification               */
  char gps_datestamp[16];       /* "YYYY:MM:DD" (UTC)                       */
  char gps_timestamp[16];       /* "HH:MM:SS"   (UTC)                       */
} media_exif;

/* Parse EXIF out of an in-memory image. Accepts JPEG (APP1/Exif), bare TIFF
 * and PNG (eXIf chunk); anything else, including HEIC/AVIF, returns 0.
 *
 * Returns 1 when an Exif block was found and walked (which does NOT imply any
 * particular field was populated — check `have_gps`, `make[0]`, …), 0 when
 * there is no EXIF or the structure is unusable. `out` is fully zeroed on
 * entry and is safe to read on either outcome.
 *
 * Total function: no allocation, no I/O, no globals, re-entrant, and — this is
 * the whole point — it cannot read outside [buf, buf+len) for ANY input. */
int media_exif_parse(const void *buf, size_t len, media_exif *out);

/* Render a media_exif as the JSON stored in media_assets.exif_json:
 *   {"gps":{"lat":…,"lon":…,"alt_m":…}, "camera":{"make":…,"model":…,
 *    "software":…,"lens":…}, "time":{"original":…,"digitized":…,"modified":…,
 *    "gps_date":…,"gps_time":…}, "image":{"width":…,"height":…,
 *    "orientation":…}, "byte_order":"II"|"MM", "truncated":bool}
 * Absent groups are omitted entirely rather than filled with nulls, so the
 * document stays small on the overwhelming majority of images that carry only
 * a couple of tags. NULL only on OOM. Caller frees. */
char *media_exif_to_json(const media_exif *e);

/* Pixel dimensions straight out of the compressed frame header — JPEG SOFn,
 * PNG IHDR, GIF LSD, BMP DIB, WebP VP8, VP8L and VP8X. Returns 1 and sets the
 * two out params, 0 when the format is unknown or the header is short. This is the
 * TRUE geometry; media_exif.width/height are metadata tags and can disagree
 * with the pixels (a cropped image whose tags were not rewritten). */
int media_image_dims(const void *buf, size_t len, long *w, long *h);

/* Magic-byte content type: "image/jpeg", "image/png", "image/gif",
 * "image/webp", "image/bmp", "image/tiff", "image/heic", "image/avif", or
 * NULL when the bytes are not a recognised image. Returns a static string.
 * Deliberately ignores any server-supplied Content-Type: what an image IS is a
 * property of its bytes, and a mislabelled response must not steer the parser. */
const char *media_sniff_type(const void *buf, size_t len);

/* ── perceptual hash ─────────────────────────────────────────────────────── */

/* THE HASH DEFINITION, so two implementations can agree:
 *   input   32x32 8-bit grayscale, row-major, 1024 bytes
 *   DCT-II  separable, unnormalised: C[v][u] = ΣΣ p[y][x]
 *           cos(π(2x+1)u/64) cos(π(2y+1)v/64)
 *   take    the top-left 8x8 of C
 *   median  of those 64 coefficients EXCLUDING the DC term C[0][0]
 *   bit i   (i = v*8 + u, MSB-first: bit i occupies 1<<(63-i)) is set when
 *           C[v][u] > median. The DC term participates in the BITS but not in
 *           the MEDIAN — the standard formulation, restated here because the
 *           variants that include DC in the median produce different, silently
 *           incomparable hashes.
 * Returns 0 — the "no usable hash" sentinel — for a NULL buffer and for a
 * FEATURELESS image. The second case is deliberate and matters: a solid-colour
 * tile (blank scan, placeholder, upscaled tracking pixel — all present in this
 * corpus) has mathematically zero AC energy, so its coefficients are floating-
 * point noise around zero and the median falls inside that noise. Every bit
 * would then be decided by accumulation order: the hash would be stable for
 * one binary, different for the next, and — worse — two identical blank images
 * would get two different fingerprints instead of matching. Such images are
 * detected (max |AC| <= 1e-6 * |DC|; real images sit thirteen orders above
 * that) and reported as "no hash", so the column is NULL rather than
 * confidently wrong. */
uint64_t media_phash_gray32(const unsigned char *px1024);

/* Hash a file by shelling out for the pixels. Returns 1 and sets *out on
 * success; returns 0 — leaving *out untouched — when no decoder tool is
 * available, when MEDIA_NO_TOOLS is set, when the path fails validation, or
 * when the tool fails or returns the wrong number of bytes. Never a fatal
 * outcome: the caller simply stores NULL. */
int media_phash_file(const char *path, uint64_t *out);

/* Hamming distance, 0..64. Two images are "the same picture" at <= 10 with the
 * usual caveat that this is a threshold on a heuristic, not an equality test. */
int media_phash_distance(uint64_t a, uint64_t b);

/* Assets within `max_distance` of `phash`, nearest first:
 *   {"data":[{id,item_uid,url,sha256,phash,distance}],
 *    "page":{"limit":n,"count":n},"meta":{"scanned":n,"max_distance":d}}
 * Implemented as a BOUNDED LINEAR SCAN of media_assets.phash — stated plainly
 * because it is the honest shape of the thing: unlike simhash.c there is no
 * banded LSH index here, and building one is only worth it once the corpus
 * actually holds enough hashed images to matter (it currently holds none; see
 * "NOT EXERCISED"). `meta.scanned` reports how many rows were examined so a
 * caller can see when the bound is biting. NULL on bad args. Caller frees. */
char *media_find_similar(db_handle *db, uint64_t phash, int max_distance,
                         int limit);

/* ── OCR ─────────────────────────────────────────────────────────────────── */

/* OCR a file by shelling out. Returns 1 and sets *out_text (malloc'd, caller
 * frees) when text was recovered. *out_conf receives mean per-word confidence
 * in 0..100 when the tool reports one (tesseract TSV mode does; a
 * MEDIA_OCR_CMD override does not) and is set to a NEGATIVE value when no
 * confidence is available — that is the signal to store SQL NULL rather than a
 * made-up number. Returns 0 with *out_text untouched when no tool is present,
 * MEDIA_NO_TOOLS is set, the tool fails, or nothing legible came back. */
int media_ocr_file(const char *path, char **out_text, double *out_conf);

/* Which external tools this host actually has, for an ops route:
 *   {"data":{"exif":{"available":true,"builtin":true},
 *            "dimensions":{"available":true,"builtin":true},
 *            "phash":{"available":bool,"tool":"…"|null},
 *            "ocr":{"available":bool,"tool":"…"|null,"confidence":bool}}}
 * Always non-NULL. Caller frees. Answers "why is every phash column NULL"
 * without an SSH session. */
char *media_capabilities(void);

/* ── pipeline ────────────────────────────────────────────────────────────── */

/* Every table, column and index this module needs, in dependency order.
 * Idempotent, mutex-guarded, runs its DDL at most once per process, and never
 * fails loudly — a DDL failure degrades the module to a no-op rather than
 * taking down whatever called it. */
void media_migrate(db_handle *db);

/* Find image URLs on ONE item and record them (INSERT OR IGNORE against
 * UNIQUE(item_uid,url)), then stamp intel_items.media_scanned_at so the item
 * leaves the discovery index. Returns the number of NEW assets recorded, or
 * -1 if the uid has no row. No network. */
int media_discover_for_item(db_handle *db, const char *item_uid);

/* One discovery batch: up to `limit` (<=0 → MEDIA_DISCOVER_BATCH) items with
 * media_scanned_at IS NULL, newest first via idx_intel_items_media_pending.
 * Returns assets recorded. `cancel` may be NULL; when non-NULL it is polled
 * between items and stops the batch cleanly. No network — pure local work, so
 * the whole batch is one short transaction. */
int media_discover_batch(db_handle *db, int limit, volatile int *cancel);

/* One analysis batch: up to `limit` (<=0 → MEDIA_ANALYZE_BATCH) assets with
 * analyzed_at IS NULL and analyze_failed < MEDIA_MAX_FAILURES.
 *
 * Discipline, mirroring core/entity_enrich.c and core/translate.c: the pending
 * batch is read and the statement FINALIZED before the first fetch, so no read
 * cursor is held across a 15-second network call; each asset's writes are
 * their own short transaction; a failing asset gets analyze_failed incremented
 * and is abandoned for good at MEDIA_MAX_FAILURES rather than burning a batch
 * slot forever; an asset that turns out not to be an image at all is stamped
 * analyzed (with content_type and bytes recorded) rather than failed, because
 * that is a fact about it, not an error.
 *
 * `http` may be NULL, in which case a private client is created and freed —
 * pass ctx->http from a pod so the connection/DNS/TLS share is reused.
 * Returns assets successfully analyzed. */
int media_analyze_batch(db_handle *db, http_client *http, int limit,
                        volatile int *cancel);

/* Write an EXIF coordinate onto an item. Sets lat/lon, geom_source='exif' and
 * geom_at, and returns 1 only if it wrote.
 *
 * NEVER OVERWRITES: the UPDATE carries `AND lat IS NULL AND lon IS NULL AND
 * geom_source IS NULL`, so a native coordinate from the collector, an LLM
 * geocode and an earlier EXIF fix are all left exactly as they are. There is
 * no force flag and there should not be one — silently moving a pin an analyst
 * has already seen is worse than not placing it. */
int media_geo_backfill(db_handle *db, const char *item_uid, double lat,
                       double lon);

/* Re-apply EXIF coordinates that a re-ingest wiped (see the geom_source hazard
 * above). Reads GPS back out of media_assets.exif_json via json_extract, driven
 * by idx_media_assets_exif, and writes it through media_geo_backfill() — so it
 * inherits the never-overwrite rule and can only ever fill a hole. Examines at
 * most `limit` (<=0 → MEDIA_GEO_BATCH) assets. Returns coordinates restored;
 * 0 is the normal steady state. */
int media_geo_reapply(db_handle *db, int limit, volatile int *cancel);

/* Rebuild intel_items_fts.keywords for items whose OCR text is missing from
 * the index — the repair half of the co-write with core/translate.c described
 * at length above. Reads the current keywords back and writes only when the
 * OCR payload is genuinely absent, so a settled corpus does zero writes.
 * Examines at most `limit` (<=0 → MEDIA_FTS_BATCH) items per call, cycling
 * through the corpus with a cursor kept in media_state; the cursor wraps when
 * it runs off the end, so this is a loop, not a one-shot migration. Returns
 * FTS rows rewritten. No LLM, no network. */
int media_fts_sync(db_handle *db, int limit, volatile int *cancel);

/* Assets for one intel item, newest-analyzed first:
 *   {"data":[{id,url,sha256,bytes,content_type,width,height,phash,exif,
 *             ocr_text,ocr_conf,analyzed_at,analyze_failed,has_gps,lat,lon}],
 *    "page":{"limit":n,"count":n},
 *    "meta":{"item_uid":…,"analyzed":n,"pending":n,"failed":n,"geotagged":n}}
 * `exif` is the parsed object, re-inlined rather than handed back as an
 * escaped string. `phash` is a 16-CHAR HEX STRING, not a number: it is a full
 * 64-bit value and JSON numbers are IEEE doubles, so emitting it numerically
 * would silently corrupt the low bits in every JavaScript client. NULL →
 * caller replies 500. limit defaults to 50, clamped 1..200. Caller frees. */
char *media_list_for_item(db_handle *db, const char *item_uid, int limit);

#endif /* JO_MEDIA_H */
