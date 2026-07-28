/* core/docmeta.h — bytes-in forensic document metadata. One call takes a
 * buffer that arrived from somewhere untrustworthy and returns everything the
 * document says about who made it, with what, and when.
 *
 * ══ WHY THIS EXISTS ═══════════════════════════════════════════════════════
 *
 * The corpus already has a PDF metadata extractor
 * (collectors/sources/pdf_analyzer.c) and a document analyzer
 * (collectors/sources/document_analyzer.c), and neither can be used for the
 * thing we actually keep needing. Both are URL/path driven: pdf_analyzer.c
 * fetches a URL and parses the response, document_analyzer.c fopen()s a path
 * and reads the first 64 KiB. So a document that is ALREADY IN MEMORY — an
 * upload, an evidence blob, a ZIP member, an email attachment, a body already
 * pulled by httpclient for some other reason — cannot be analysed without
 * writing it back out to a file or re-fetching it over the network. This
 * module is the same extraction with the I/O removed: buffer in, JSON out.
 *
 * The OSINT payload is the point. A leaked PDF names its author, the exact
 * Word/InDesign/Ghostscript build that produced it, and two timestamps. A
 * leaked .docx names its author, the LAST PERSON TO SAVE IT (cp:lastModifiedBy
 * — frequently a different, more interesting person than the author), the
 * organisation that licensed Office (docProps/app.xml Company), and the total
 * editing time. That is attribution evidence sitting in plaintext inside files
 * people publish without a thought, and it is cheap to read.
 *
 * ══ THE THREAT MODEL, BECAUSE IT DRIVES EVERY DESIGN CHOICE ═══════════════
 *
 * These bytes are hostile. They are whatever an unauthenticated client, or a
 * random web server, decided to hand us. The contract of this module is
 * absolute, and matches the one core/media.h's EXIF parser sets:
 *
 *   THERE IS NO INPUT FOR WHICH THIS CODE READS OUTSIDE [buf, buf+len),
 *   ALLOCATES AN ATTACKER-CHOSEN AMOUNT, OR SCANS AN ATTACKER-CHOSEN AMOUNT.
 *
 * Concretely:
 *
 *  - The buffer is NOT a C string and is never treated as one. No strlen,
 *    strstr, strchr or sscanf ever touches it; every search is the bounded
 *    mem_find() in docmeta.c. This is not a stylistic preference — the two
 *    existing extractors both rely on a NUL that a 64 MiB binary upload will
 *    not contain (document_analyzer.c gets away with it only because it
 *    writes the terminator itself after a bounded fread).
 *  - Every bound is written `n <= len - off` after proving `off <= len`, never
 *    `off + n <= len`, which is the same statement only while the addition
 *    does not wrap. Copied deliberately from media.c's ex_have().
 *  - Every extracted string is decoded to real UTF-8, control-scrubbed, and
 *    truncated ON A CODEPOINT BOUNDARY before it reaches cJSON. A /Author
 *    value containing 0x1B, a lone 0xFF, or 400 KiB of text is a crafted file,
 *    and none of those three belong in a JSON document or a UI.
 *  - Nothing is fabricated. A field that could not be read is ABSENT, not
 *    empty-string, not zero, not guessed. An unrecognised buffer returns its
 *    type as "unknown" plus a hex prefix of its first bytes so an analyst can
 *    identify it by eye, which is strictly more useful than a wrong label.
 *
 * ══ WORK CAPS — the answer to "what does a 64 MiB file cost" ══════════════
 *
 * Every scan is windowed. The caps are the DOCMETA_* macros below and they are
 * the whole story:
 *
 *  PDF      Each of the ~13 keyed lookups (8 Info keys + /Encrypt, /Linearized,
 *           /JavaScript, /EmbeddedFiles, /AcroForm) scans at most a 1 MiB HEAD
 *           window and a 1 MiB TAIL window — never the middle. That is where
 *           PDF metadata physically lives: the header and the first object at
 *           the front, the trailer and the Info reference at the back. So the
 *           keyed work is bounded at ~26 MiB of byte comparison for ANY input
 *           size, and at 2*len for small files. Page counting is a separate
 *           single pass over at most an 8 MiB head window (two patterns, so
 *           16 MiB), flagged "page_count_partial" when the file is bigger.
 *           TOTAL WORST CASE: ~42 MiB of comparisons, zero heap growth.
 *  ZIP      At most DOCMETA_ZIP_MAX_ENTRIES central-directory entries walked
 *           per lookup, at most 3 walks. The EOCD search is bounded to the
 *           last 66000 bytes (max ZIP comment + record).
 *  INFLATE  Output is capped at DOCMETA_ZIP_MAX_UNCOMP into ONE fixed
 *           allocation that never grows. See the zipread note below.
 *  TEXT     Line/encoding analysis over a 4 MiB head window; HTML tag scan
 *           over a 256 KiB head window.
 *  VALUES   DOCMETA_RAW_MAX bytes captured per raw value, DOCMETA_MAX_VALUE
 *           bytes of UTF-8 kept.
 *
 * ══ WHY lib/zipread.h IS NOT USED, HAVING READ IT ═════════════════════════
 *
 * zipread.c is correct for what it was written for and it is NOT being
 * criticised as buggy: its bounds hold, and it walks the central directory for
 * exactly the right reason (a streaming data descriptor leaves the sizes zero
 * in the local header). It is unusable HERE for one reason, memory:
 *
 *   1. inflate_raw() pre-allocates the file-declared uncompressed size
 *      (`capacity = uncomp + 1`). That field is 32 bits and attacker-chosen,
 *      so a 300-byte .docx that declares 4 GiB is a 4 GiB malloc.
 *   2. When the declared size is a lie in the other direction, the output
 *      buffer DOUBLES until the stream ends. There is no ceiling. That is the
 *      classic deflate bomb: a few KB of zeros expands without limit.
 *   3. It returns the WHOLE entry, so even an honest 200 MiB member is a
 *      200 MiB allocation.
 *
 * None of that matters for GDELT and GTFS-JP, which is what zipread serves —
 * those are known feeds from known hosts. It matters completely for a buffer a
 * client uploaded. So this module drives zlib's raw inflate directly with a
 * FIXED, NON-GROWING DOCMETA_ZIP_MAX_UNCOMP output buffer and stops the moment
 * it is full (marking "truncated"), which makes a bomb cost exactly 1 MiB and
 * one bounded inflate. The central-directory walk here is likewise bounded by
 * entry count and re-validates every offset against the buffer.
 *
 * This is a deliberate ~120-line overlap with lib/zipread.c, taken with eyes
 * open because the alternative was either an unbounded allocation on hostile
 * input or throwing away the highest-value field in the whole module
 * (cp:lastModifiedBy). If someone later gives zipread.c a caller-supplied
 * output ceiling, DELETE the zip section here and call it instead.
 *
 * ══ WHAT IS PARSED VS WHAT IS ONLY LABELLED ══════════════════════════════
 *
 * PARSED (real fields come out):
 *   PDF                    header version, Info dict (/Title /Author /Subject
 *                          /Keywords /Creator /Producer /CreationDate
 *                          /ModDate), approximate page count, /Encrypt,
 *                          /Linearized, /JavaScript, /EmbeddedFiles, /AcroForm
 *   docx / xlsx / pptx     docProps/core.xml (dc:creator, dc:title,
 *                          dc:subject, dc:description, cp:keywords,
 *                          cp:lastModifiedBy, cp:revision, cp:category,
 *                          dcterms:created, dcterms:modified) and
 *                          docProps/app.xml (Application, AppVersion, Company,
 *                          Manager, Template, TotalTime, Pages, Words)
 *   text / csv / json      byte and line counts, encoding, BOM, line endings;
 *                          CSV delimiter and column count
 *   html                   <title>, <meta name=generator|author|description>
 *
 * LABELLED ONLY (type reported honestly, no fields invented):
 *   png / jpeg / gif       core/media.h owns images. media_exif_parse() and
 *                          media_image_dims() already do this properly and
 *                          duplicating them here would be a second, worse
 *                          EXIF parser. docmeta_sniff() identifies them so a
 *                          caller can route to media.h.
 *   plain zip              a ZIP that is not OOXML: container + entry count.
 *   ole2 / rtf / gzip /    recognised magic reported in "magic_note" so a
 *   7z / rar / elf / pe    legacy .doc or an encrypted OOXML (which is an OLE2
 *                          container, NOT a ZIP) is identifiable rather than
 *                          silently "unknown". No parsing is attempted.
 *
 * NOT DONE, stated so nobody assumes otherwise:
 *   - Legacy OLE2/CFB .doc/.xls/.ppt property streams are not parsed. That is
 *     a real compound-file parser, not a header peek.
 *   - PDF object streams (/ObjStm) are not inflated, so a modern PDF that
 *     compresses its page tree will report no page count rather than a wrong
 *     one, and an XMP-only PDF with no Info dict will report no author.
 *   - Encrypted PDFs: /Encrypt is reported, but the Info strings of an
 *     encrypted PDF are ciphertext. When /Encrypt is present the extracted
 *     values are still returned (they are often readable in practice, since
 *     many producers leave the Info dict unencrypted) but "encrypted" is set
 *     and a note says the strings may be ciphertext. They are not decrypted.
 *   - Incremental updates: the FIRST plausible value found wins, head window
 *     before tail. A PDF revised in place can therefore surface an EARLIER
 *     revision's author, which is forensically interesting but is not
 *     necessarily what a PDF reader displays.
 *
 * ══ SCHEMA ════════════════════════════════════════════════════════════════
 *
 * NONE. No tables, no columns, no indexes, no migration. This module is a pure
 * function over bytes: no db_handle, no network, no globals, no static state,
 * re-entrant and thread-safe.
 *
 * ══ WIRING THE ORCHESTRATOR MUST DO ═══════════════════════════════════════
 *
 * NONE IS REQUIRED. The Makefile globs the core .c files so this links in, and
 * nothing calls it until someone wants it. Two suggested call sites:
 *
 *  1. core/httpd.c — analyse an uploaded document. hm->body is already a
 *     length-delimited buffer, which is exactly this module's input shape:
 *
 *       POST /api/docmeta                 (plain auth; no tenant data touched)
 *         char *m = docmeta_extract(hm->body.buf, hm->body.len, NULL,
 *                                   header_value(hm, "Content-Type"));
 *         char *out = malloc(strlen(m) + 16);
 *         sprintf(out, "{\"data\":%s}", m);   / * envelope, per CONVENTIONS * /
 *         reply_json(c, 200, out); free(m); free(out); return;
 *
 *     Note the envelope is applied by the CALLER. docmeta_extract() returns the
 *     bare object because it is a library function, not a handler — it has no
 *     opinion about whether it is being wrapped in {"data":…}, stored in a
 *     column, or embedded in an intel item's properties.
 *
 *  2. core/evidence.c or any collector holding a fetched body: gate on the
 *     REAL type before doing anything expensive —
 *
 *       if (strcmp(docmeta_sniff(body, n), "pdf") == 0) { … }
 *
 *     rather than on a Content-Type header, which is a claim, not a fact.
 *
 * ══ OUTPUT SHAPE ══════════════════════════════════════════════════════════
 *
 *   {"type":"pdf","bytes":48213,"sniffed":"255044462d312e370a25…",
 *    "filename":"leak.pdf","content_type_claimed":"application/pdf",
 *    "pdf":{"version":"1.7","title":"…","author":"…","producer":"…",
 *           "creator":"…","subject":"…","keywords":"…",
 *           "created":"2019-03-04T11:22:33+09:00",
 *           "created_raw":"D:20190304112233+09'00'",
 *           "modified":"…","modified_raw":"…","dates_normalized":"iso-8601",
 *           "approx_pages":12,"encrypted":false,"linearized":true,
 *           "has_javascript":false,"scan_windowed":false}}
 *
 *   {"type":"docx","bytes":18422,"sniffed":"504b0304…",
 *    "office":{"container":"zip","format":"docx","entries":14,
 *              "core":{"creator":"h.tanaka","last_modified_by":"y.sato",
 *                      "title":"…","revision":"7",
 *                      "created":"2024-11-02T04:15:00Z",
 *                      "modified":"2025-01-19T22:41:00Z"},
 *              "app":{"application":"Microsoft Office Word",
 *                     "company":"…","total_edit_minutes":184,"words":3120}}}
 *
 *   {"type":"unknown","bytes":9,"sniffed":"d0cf11e0a1b11ae1000000",
 *    "magic_note":"ole2-cfb — legacy Office document or encrypted OOXML;
 *                  not parsed"}
 *
 * Absent fields are OMITTED, never null. That is the honest-empty rule: the
 * difference between "this document has no author" and "we could not read the
 * author" is preserved by the presence or absence of the key, and nothing
 * downstream has to guess.
 */
#ifndef JO_DOCMETA_H
#define JO_DOCMETA_H

#include <stddef.h>

/* ── work caps (see "WORK CAPS" above) ───────────────────────────────────── */

/* Bytes examined at the head, and again at the tail, for each PDF key. A key
 * is never searched for in the middle of a large file. */
#define DOCMETA_PDF_WINDOW      ((size_t)1 << 20)

/* Head bytes scanned when counting "/Type /Page". Separate and larger than the
 * key window because page objects are spread through the body, not clustered
 * at either end; "page_count_partial" is set when the file exceeds it. */
#define DOCMETA_PAGE_SCAN       ((size_t)8 << 20)

/* Head bytes scanned for line counting / encoding detection. */
#define DOCMETA_TEXT_SCAN       ((size_t)4 << 20)

/* Head bytes scanned for HTML <title> and <meta>. */
#define DOCMETA_HTML_SCAN       ((size_t)256 << 10)

/* Bytes examined to decide whether a buffer is text at all. */
#define DOCMETA_SNIFF_WINDOW    ((size_t)4096)

/* Candidate occurrences of one key examined before giving up. Stops a file
 * that is 1 MiB of "/Author" with no value from costing a parse attempt each. */
#define DOCMETA_MAX_KEY_HITS    64

/* <meta> tags examined in an HTML head window. */
#define DOCMETA_MAX_META_TAGS   256

/* Raw bytes captured for one value BEFORE decoding, and UTF-8 bytes kept
 * after. Both are hard truncations; the second lands on a codepoint boundary. */
#define DOCMETA_RAW_MAX         2048
#define DOCMETA_MAX_VALUE       512

/* Central-directory entries walked per ZIP lookup. */
#define DOCMETA_ZIP_MAX_ENTRIES 4096

/* Hard ceiling on inflated output for ONE ZIP member, in ONE allocation that
 * never grows. This is the deflate-bomb containment — see the zipread note. */
#define DOCMETA_ZIP_MAX_UNCOMP  ((size_t)1 << 20)

/* Leading bytes hex-dumped into "sniffed". */
#define DOCMETA_HEX_PREFIX      16

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Best-effort type label from the MAGIC BYTES of a buffer, one of:
 *   "pdf" "docx" "xlsx" "pptx" "zip" "png" "jpeg" "gif"
 *   "text" "html" "json" "csv" "unknown"
 * Returns a static string and NEVER NULL — a NULL buffer, a zero length or an
 * unrecognisable one all yield "unknown".
 *
 * Exists so a caller can gate on what a buffer IS rather than on what a client
 * SAID it is. A Content-Type header is a claim from the other end of a socket;
 * these are the file's own bytes. Where the two disagree, this is right.
 *
 * "docx"/"xlsx"/"pptx" require walking the ZIP central directory (bounded by
 * DOCMETA_ZIP_MAX_ENTRIES), so this is cheap but not free on ZIP input; every
 * other answer is decided from at most DOCMETA_SNIFF_WINDOW head bytes.
 * "text"/"html"/"json"/"csv" are heuristics over that window and are labelled
 * as heuristics in docmeta_extract()'s output. */
const char *docmeta_sniff(const void *buf, size_t len);

/* Sniff the buffer and extract every metadata field the format gives up.
 *
 * `filename_hint` and `content_type_hint` are HINTS ONLY and may be NULL. They
 * are echoed into the result (scrubbed and truncated) and used to set
 * "extension_mismatch" / "content_type_mismatch", which is itself a finding —
 * a .jpg that is really a ZIP, or an "application/pdf" upload whose bytes are
 * an ELF binary, is exactly what you want flagged. They NEVER steer the
 * parser: dispatch is on magic bytes alone.
 *
 * Returns a malloc'd JSON OBJECT (no {"data":…} envelope — the caller decides
 * that; see WIRING). Caller frees. Never NULL for any input, including a NULL
 * buffer, a zero length and a 1-byte buffer; the only path that could return
 * NULL is total malloc exhaustion of a 32-byte fallback allocation, at which
 * point the process is already finished.
 *
 * No I/O, no network, no database, no globals, no static state. Thread-safe
 * and re-entrant. Cost is bounded by the DOCMETA_* caps for ANY input. */
char *docmeta_extract(const void *buf, size_t len,
                      const char *filename_hint,
                      const char *content_type_hint);

#endif /* JO_DOCMETA_H */
