<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Rule 4c: one manifest parser, probe-tool traps, TLS intermediates, registry.faa.gov. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

### 4c. Two tools reading one manifest must read it the same way

`OSM_API_CHANGESETS_BLACKSEA` carried `interval=86400\;pagination_ok=…;interval=86400`.
`gen_hp_batch.py` resolved the duplicated key last-wins and generated correct C;
`audit_batch_reachable.py` resolved it first-wins, and reported a row that runs
daily as one that can never run (house rule 3). Seven tools read these
manifests and each brought its own parser.

There is now exactly one: **`native/tools/manifest.py`**. It does not resolve a
duplicate at all — it reports it, and every reader refuses to guess
(`gen_hp_batch.py` rejects the row, the auditors call it UNVERIFIABLE). It also
hands back the lines that LOOK like rows and are not, because "skipped" and
"checked and clean" used to be the same output.

Two more of the same family turned up in batch 33, both in
`tools/probe_hp_batch.py`, and both made it report a source DEAD that the
engine measurably handles — which matters more than the reverse, because the
probe is the tool that decides whether a row ships:

* its anchor regex was `href\s*=\s*["\']([^"\'#]+)["\']`, and that class stops
  at the `#` and then demands the closing quote right there, so an href
  carrying a fragment matched NOTHING. Every anchor in a PEP 503 simple index
  ends `...tar.gz#sha256=<64 hex>`, so `pypi.org/simple/requests/` scored 0
  anchors on a page holding 244 and came back UNPARSEABLE. hpengine reads the
  whole attribute and keeps them.
* its declared-`array_path` resolver walked dict keys only, so it understood
  neither `.` (the root) nor a path that crosses an array, and four live rows
  came back `PATH_UNRESOLVED`. `_resolve_array_path()` now mirrors `hp_path`
  and `hp_path_multi`.
* **and a THIRD of the same family, at the transport layer**: Python's
  `ssl.create_default_context()` refuses handshakes the ENGINE completes.
  `yamaguchi-opendata.jp`, which carries 24 registered CKAN rows, answers
  `TLSV1_ALERT_INSUFFICIENT_SECURITY` to the default context and **HTTP 200 at
  `DEFAULT@SECLEVEL=1` with verification still on**, while the engine stores
  102 records from it across 3 requests. So the host is alive and only the tool
  could not see it. **`tools/jo_tls.py` is now THE context** for this
  directory, for rule 4c's reason — two copies of a thing diverge — and it
  ASSERTS that verification and hostname checking stay on, because lowering a
  cipher security level is a different act from skipping verification and must
  not become it.

  The historical cost of that strictness was then measured rather than assumed:
  eight batch-reject files name a TLS refusal, 51 candidates between them, and
  all 51 were re-asked (`docs/tls-rejects-revisited-2026-10-20.tsv`). **21
  alive, 30 still failing — and the cost to the registry is ZERO**: 20 of the
  21 are the `kkj.go.jp` corpus this session re-registered properly anyway, the
  21st serves five posts titled "Sample post5" from a demo WordPress install,
  and the 30 fail certificate verification for the engine too (measured on
  six: IODA x3, Dataverse UCLA, NADA Nepal, NI MARENA, each `rc=-1` with a
  fetch-failure record). Worth both numbers: the defect was real and was
  mislabelling 20 live registered rows as dead, and it had cost no source its
  place.

  **And that "30 still failing" was wrong about 24 of them, because it read one
  MESSAGE as one cause** (2026-10-21,
  `docs/tls-intermediates-2026-10-21.md`). `unable to get local issuer
  certificate` does not say the certificate is untrustworthy; it says no client
  here could find the certificate that SIGNED it. A public CA signs a leaf with
  an intermediate and the server is supposed to send that intermediate too —
  nine hosts send only the leaf, and neither OpenSSL nor libcurl will go and
  fetch the missing link, which is why a browser accepts the same page. The
  link is public and the leaf itself names it, in its AIA `CA Issuers`
  extension. Seven distinct intermediates, each verified against the SYSTEM
  root store before it was kept, now live in
  **`native/data/tls/extra-intermediates.pem`** with their provenance, loaded by the
  engine (`core/catrust.c`, through `CURLOPT_SSL_CTX_FUNCTION`) and by the
  tools (`tools/jo_tls.py`). Measured through `http_request()` itself: **nine
  of nine hosts go from `rc=1 status=0` to HTTP 200**, and the 24 candidates on
  them re-probe 24 of 24 PASS.

  **It cannot widen trust, and that is structural rather than promised.** Every
  entry is an intermediate — signed by another CA, never self-signed — and
  OpenSSL accepts a chain only when it ends at a self-signed certificate that
  was already trusted (`X509_V_FLAG_PARTIAL_CHAIN` is not set). So an entry can
  COMPLETE a chain and never ANCHOR one. `tests/unit/test_catrust.c` fails the
  build on a self-signed, expired or unchainable entry, checks the
  self-signed discriminator against 50 real roots so it cannot pass vacuously,
  and all three refusals were confirmed by hand rather than assumed.

  So: **read the TLS error, not the word "TLS".** "No local issuer",
  "expired", "hostname mismatch" and "self-signed in chain" are four findings
  with four remedies, and only one is about a certificate we should not trust.
  Of the original thirty, 24 were a missing intermediate, 2 re-point to another
  name (`www.ip.kyusan-u.ac.jp` → `www.kyusan-u.ac.jp`; `matrix.kde.org`
  delegates to `kde.modular.im`), 1 was this container's own egress, 1 was the
  already-refuted Yamaguchi 409, and **2 are genuinely unreachable** — an SNI
  mismatch at `dataverse.openscholar.org` and `nesstar.statssa.gov.za`, which
  only not checking the hostname would reach.

  **BUT CHECK WHOSE CERTIFICATE YOU ARE LOOKING AT FIRST, BECAUSE IN A PROXIED
  CONTAINER IT IS NEVER THE UPSTREAM'S** (2026-10-25). The AIA method above —
  read the leaf's `CA Issuers` extension, fetch the missing intermediate,
  verify it against the system root store — assumes the leaf on the wire is the
  publisher's. Here it is not. Measured with `openssl s_client` against six
  hosts including three that work perfectly (`github.com`, `laws.e-gov.go.jp`,
  `catalogue.dataspace.copernicus.eu`): **every one presents a certificate
  issued by `O = Anthropic, CN = Egress Gateway SDS Issuing CA (production)`**,
  and the proxy's CA is in the trust bundle (14 Anthropic entries of 148 in
  `/root/.ccr/ca-bundle.crt`, which is also why its own status reports
  `bundleCoversEveryHost: true`). TLS terminates at the egress gateway for
  every host, so the upstream's real chain is not observable from this
  container at all.
  So a `no_local_issuer` reading taken HERE is about the chain the gateway
  presented, never about the publisher's own, and following D1's remedy from
  such a reading would add intermediates that have nothing to do with the
  cause — a confident wrong fix, in the one area of this tree where a wrong
  certificate is worst. It does not retract D1: those seven intermediates were
  each verified against the system root store and took nine hosts from
  `rc=1 status=0` to HTTP 200, which is a measurement. It bounds it. The seven
  rows the complete sweep's re-read reported as `no_local_issuer` — five
  `sci-gwosc-*` on one host, plus `apdrc.soest.hawaii.edu` and
  `nhgeodata.unh.edu` — are therefore **unmeasured from here**, not diagnosed.
  Before trusting any TLS cause, run one `openssl s_client` against a host you
  know works and read the issuer.

  **THE ENGINE NOW TELLS THEM APART, which was open item D1-bis** (2026-10-23).
  The disclosure used to read "the request failed at the transport level",
  which is the paragraph above's own lesson un-learned at the one place a
  reader meets it: 24 of 30 hosts were a fixable missing intermediate and
  nothing in the record said so. `http_last_tls_cause()`
  (`core/httpclient.{c,h}`) names nine causes — `no_local_issuer`, `expired`,
  `not_yet_valid`, `hostname_mismatch`, `self_signed`, `revoked`,
  `unrecognized_name`, `handshake`, `other` — and both fetch-failure records
  (hpengine's and `core/scheduler.c`'s, since rule 1's "look for the other
  copies" applies here too) carry `tls_cause`, `tls_detail` and a cause-specific
  `remedy`. Three things about it are deliberate:

  * **the discriminator is OpenSSL's own verify result**
    (`CURLINFO_SSL_VERIFYRESULT`, an `X509_V_ERR_*` code), not the message
    text, because a reworded or translated libcurl string would silently
    reclassify every host. The text is read ONLY where there is no verify
    result — a hostname mismatch arrives that way on some backends — and
    `test_http_tls_cause.c` pins that the verify result wins when the two
    disagree;
  * **a handshake refused before any certificate was judged is not a trust
    finding** and must not be reported as one, so `CURLE_SSL_CONNECT_ERROR`
    with nothing to go on is `handshake`, whose remedy points at
    `tools/jo_tls.py`'s cipher security level rather than at a certificate;
  * **`unrecognized_name` is named apart from `handshake` although libcurl
    reports both as CURLE 35**, because the remedies are opposite. Measured
    2026-10-23: the two "genuinely unreachable" hosts above both answer
    `tlsv1 unrecognized name` — OpenSSL alert 112, the server saying it does
    not serve that hostname at all — and no cipher or trust change reaches
    that. A remedy that names a knob which cannot move is worse than none.

  The `[http]` log line now carries libcurl's own sentence
  (`CURLOPT_ERRORBUFFER`) beside the class, because `curl_easy_strerror` gives
  "SSL peer certificate … was not OK" where the buffer gives "unable to get
  local issuer certificate". The nine causes' labels and remedies were checked
  against the real messages on the wire the same day (badssl.com's expired,
  wrong-host, self-signed and untrusted-root hosts, plus the two SNI hosts).

* **`registry.faa.gov` is gone, and the measurement is why.** Three rows read
  it — `US_FAA_AIRCRAFT_REGISTRY`, `FAA_NNUMBER_REGISTRY` and `FAA_REGISTRY`
  (a bespoke collector in `collectors/sources/vehicles_world.c`) — and all
  three are removed as of 2026-10-21. One request per agent that day: the
  engine's `JO_USER_AGENT` 403, the same agent with `feed collector` stripped
  403, `curl/8.5.0` 403, **`JapanOSINT/1.0` alone 200**, no User-Agent at all
  200. So it is neither a one-word token filter nor a block on identifying
  ourselves: it refuses an agent that carries a parenthetical contact route.
  **Main's own remedy for a 403 was then tried here and refused too**: the
  `Mozilla/5.0 (compatible; JapanOSINT/1.0; +…)` header that recovered 491
  wall-blocked rows answers **403** on both faa.gov paths (measured
  2026-10-21), so this removal does not rest on one agent having been tried.
  Recovering those rows would mean dropping our contact route for that host,
  which is the one thing `core/httpclient.c`'s UA_OVERRIDE table is not for —
  and that table's own comment had already declined to list the host for a
  weaker version of the same reason. Three sources removed rather than left
  registered and failing every run.

Unifying the parser immediately found three live defects of the same family, where a
stray `\;` escaped the separator and the following opt was swallowed into the
previous VALUE — shipped into committed C as
`.date_keys = "exchangedate;pagination_ok=start/end are a DATE range…"`, a
`.detail_key` that names no field, and a User-Agent header with 130 characters
of prose glued to it. `gen_hp_batch.py` now rejects that shape by name.

Engine subtleties worth knowing before writing a row:

