# fix_lib.md — parsing/collection library fixes (audit 2026-11-14)

Offline tests (built standalone into scratchpad/bin, final run): hpengine_test, rss_atom_test, pagewalk_test, htmlparse_test: all "all passed".
New tests: tests/unit/test_lib_audit.c (ASAN/UBSan clean, all ok) + new cases in hpengine_test (LIB-03a-d, LIB-08), rss_atom_test (LIB-05, 15, 15b), htmlparse_test (LIB-02, 14). Every edited file compiles -Wall -Wextra warning-free.

| id | status | change / evidence |
|---|---|---|
| LIB-01 | FIXED | xlsx.c: pass 1 sums the CSV-encoded size per referencing cell (shared-string cost precomputed once); JO_XLSX_MAX_BYTES, default 512 MB, refuses like the cell ceiling. amp.xlsx: 3,000,003,000 B in 3 s -> refused in 0.017 s. |
| LIB-02 | FIXED | htmlparse.c: one stack pass (ht_scan_tables) gives every <table> its end; `must` located by one forward jo_memmem sweep. 140 KB 3.4 s / 560 KB 60 s -> 0.00 / 0.01 s; 2.8 MB 0.04 s. Selection semantics unchanged (tests). |
| LIB-03 | FIXED (hpengine) | hp_host_of: '@' only within authority, IPv6 literals; hp_resolve_url (relative / // / ? / path); next URL on another host (case-insens.) is followed with no row headers, no POST body, row key stripped from URL; "{v}"-whole detail URL on another host: no row headers/key. jsonlist resolve_link gained "//host". jsonlist_next_page / pagewalk / geojson use feed_get_json (no row headers at all, so no credential leak; they carry only the URL the author wrote) -> not changed beyond //. hptest cases pass; |
| LIB-04 | FIXED | csv_decode_charset maps SHIFT_JIS/SJIS/... to CP932; EINVAL keeps prefix + U+FFFD; EILSEQ still fails closed. csv_decode_sjis fixed so rss_atom/feedlib/npa_*/gov_money/chan_5ch all inherit. |
| LIB-05 | FIXED | tag_text: pass 1 unprefixed only (self-closing skipped, scan continues), pass 2 prefixed; close tag must equal the open name. **Identity note:** items whose first `<x:link/>` made link "" (uid fell to title hash) now get the real link as uid; that is the point of the fix but re-keys those rows once. |
| LIB-06 | FIXED | csv_name_rows walks ->next (80k columns linear); duplicate names get .1, .2 (hash table, never collides with a literal "a.1"). |
| LIB-07 | FIXED | collect_t searches rPh once. 60,000-run <si>: 70 s -> 0.035 s. |
| LIB-08 | FIXED (provable scope) | new header-only lib/jsonbig.h: jo_json_quote_bigints quotes only bare integer literals outside strings with >=16 digits that do NOT survive the double round trip; used at hpengine page + detail parse, feedlib feed_get_json (2 sites; covers jsonlist/pagewalk/geojson), jsonstream. Exactly-representable ints (e.g. 1.7e18 ns timestamps) stay numbers. Side effect: such lossy ids are now JSON strings in stored properties; a big-int next_path cursor without next_tmpl is not used as a URL. |
| LIB-09 | FIXED | utf8_decode: overlong/surrogate/>10FFFF -> U+FFFD adv 1; csv_is_utf8 strict (rss is_utf8 now calls it); valid text unaffected. |
| LIB-10 | FIXED | keyqual occurrences: two binary searches. (compile-checked; no standalone test: needs feedlib) |
| LIB-11 | FIXED | port_scanner, email_validator, ssl_analyzer: getaddrinfo once, every address judged with hostgate_addr_check(.,1), connect to the judged sockaddr (IPv4+IPv6). Three static copies (no shared header allowed in my file list). |
| LIB-12 | FIXED | email in Hunter URL percent-encoded; smtp_reply reads to the final line of a multi-line reply. |
| LIB-13 | FIXED | hp_scalar_str and numeric next cursor range-guarded like jsonlist. |
| LIB-14 | FIXED | digits-only numeric entities; out-of-range/surrogate -> U+FFFD; malformed stays literal. |
| LIB-15 | FIXED | rss: items with no guid/link/title counted -> one shape notice (jo_shape_notice "items-skipped-no-identity"); rfc822/ISO dates validated; invalid -> published_at NULL + `published_raw` in properties. |
| LIB-16 | FIXED | zipread: zip_iter (one directory pass), ZIP64 EOCD/locator + ZIP64 extra fields read (>65,535 members, 64-bit sizes), zip_entry_count real count, EOCD search checks comment length, inflate alloc bounded by compressed size*1100, stored member larger than archive / empty stored member no longer returns rest-of-file. hpengine zip_all_entries uses the iterator, discloses unwalkable directory members (declared > walked) via the existing failed-members notice, and fails hard on an unreadable directory. |
| LIB-18/ARC-01 | FIXED | ws.c: reassembly bounded (JO_WS_MAX_MSG_BYTES, 16 MB; oversize message dropped whole, counted in ws_last_oversize_dropped()); hostgate_url_check, hostgate_acquire, http_easy_harden (redirect cap, per-connection SSRF check), http_ua_override/JO_USER_AGENT. jsonstream.c already had hostgate+UA+harden (no change beyond LIB-08). |
| LIB-19 | FIXED | the repeat-name counter scan was O(distinct names) and ran for elements already past the 2048-property cap; skipped once capped. (40k same-name siblings were already 0.1 s non-ASAN; the O(N^2) case was many distinct names.) |
| LIB-20 | FIXED | academic_world: arXiv/PubMed/OpenLibrary author lists no longer capped (aw_join, heap); `authors_total` count added to the record. |
| LIB-17 | not mine | datetok.c excluded. |

## Needs edits outside my files
- Callers of ws_collect (bluesky/certstream collectors) should disclose `ws_last_oversize_dropped()` via a notice.
- lib/jsonlist.h / Makefile: none required (jsonbig.h header-only). tests/unit/test_lib_audit.c uses run.sh's first-include convention (csv.c); it links xlsx.o/zipread.o.
- A shared "resolve once, judge, connect" helper in core/ would replace the three static copies (LIB-11).
