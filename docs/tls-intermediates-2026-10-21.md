# The 24 "unfixable TLS" rows were a server omitting the middle of its chain

Open item D1 said thirty probed candidates failed TLS for the engine as well as
for the tools, and proposed removing them. That was the wrong diagnosis for
**24 of them**, and the error was in reading one message as one cause.

    unable to get local issuer certificate

This does NOT mean the certificate is untrustworthy. It means no client here
could find the certificate that signed it. A public CA signs a leaf with an
INTERMEDIATE, and the server is supposed to send that intermediate alongside
the leaf. Nine hosts send only the leaf. Neither OpenSSL nor libcurl will go
and fetch the missing link, so the path cannot be built and the handshake
fails — on a certificate that a browser accepts, because a browser caches
intermediates it has seen elsewhere and will fetch one on demand.

The missing link is public, and the leaf itself says where: its Authority
Information Access extension carries a `CA Issuers` URL at the issuing CA's own
site.

## Measured, 2026-10-21

For each host: fetch its leaf, read its AIA `CA Issuers` URL, fetch that, then
ask OpenSSL to verify the leaf against the SYSTEM root store with the fetched
certificate supplied as an untrusted link.

| host | leaf's issuer | AIA source | verify |
| --- | --- | --- | --- |
| app.legco.gov.hk | Hongkong Post e-Cert SSL CA 3 - 17 | www1.eCert.gov.hk | **OK** |
| www.ansd.sn | GlobalSign GCC R3 DV TLS CA 2020 | secure.globalsign.com | **OK** |
| www.iwabuchi.co.jp | GlobalSign GCC R3 DV TLS CA 2020 | secure.globalsign.com | **OK** |
| www.kanamic.net | Cybertrust Japan SureServer CA G4 | crl.cybertrust.ne.jp | **OK** |
| www.ons.dz | Sectigo Public Server Authentication CA DV R36 | crt.sectigo.com | **OK** |
| www.pcrwr.gov.pk | GoGetSSL RSA DV SSL CA 2 | crt.sectigo.com | **OK** |
| www.sanden.co.jp | GlobalSign RSA OV SSL CA 2018 | secure.globalsign.com | **OK** |
| www.suzuden.co.jp | GlobalSign GCC R3 DV TLS CA 2020 | secure.globalsign.com | **OK** |
| www.traas.co.jp | FujiSSL RSA Domain Validation Secure Server CA 2 | crt.sectigo.com | **OK** |

Nine hosts, seven distinct intermediates, every one chaining to a root that was
already in the system store.

**Read the WHOLE AIA block.** An AIA extension lists its OCSP responder first
and `CA Issuers` second, so a read of the line after the extension header finds
only the responder. `www.kanamic.net` came back `NO_AIA` from exactly that, and
so did two of the hosts in the D1-ter follow-up — three findings from one
one-line grep.

### Not every "no local issuer" is a missing intermediate

The same message has a second cause, and the depth in the error separates them:

    error 20 at 1 depth   the INTERMEDIATE's issuer is missing — the ROOT is
                          not in this store, and no AIA fetch can fix it
    error 20 at 0 depth   the LEAF's issuer is missing — this is the shape
                          above, and the leaf's own AIA names the remedy

`api.ioda.inetintel.cc.gatech.edu` and `dataverse.ucla.edu` are the first kind:
their InCommon intermediate is issued by `emSign Root TLS CA - G3` / `G1`
(eMudhra), of which this container's store holds zero. Making those work would
mean adding a new trust ANCHOR, which this bundle forbids by construction — so
they are recorded as an OS trust-store question and not worked around.

## The fix: `native/data/tls/extra-intermediates.pem`

Those seven certificates, with per-certificate provenance in the file: subject,
issuer, expiry, SHA-256, the AIA URL each came from, which hosts omit it, and
the chain `openssl verify -show_chain` built with it.

* **the engine** loads it in `core/catrust.c`, through libcurl's
  `CURLOPT_SSL_CTX_FUNCTION`, by adding each certificate to the connection's
  verification store. `CURLOPT_SSL_VERIFYPEER` and `CURLOPT_SSL_VERIFYHOST`
  are not touched and that file contains no code that could touch them;
* **the tools** load it in `native/tools/jo_tls.py`, which already asserts that
  verification and hostname checking stay on;
* the path resolves `$JO_CA_EXTRA`, then `$JO_REPO_ROOT`, then the repo the
  binary was built from — never a path written into the source, the same rule
  as the breach corpus root. A deployment with no such file behaves exactly as
  it did before, with nothing logged.

### Why this cannot widen trust, structurally

Every certificate in the bundle is an INTERMEDIATE — signed by another CA,
never self-signed. OpenSSL does not set `X509_V_FLAG_PARTIAL_CHAIN` here, so a
chain is accepted only when it ends at a self-signed certificate in the trusted
store. A non-self-signed entry can therefore COMPLETE a chain and can never
ANCHOR one: whatever it vouches for still has to be vouched for by a root that
was trusted before this file existed.

That is a property to check rather than promise, so `tests/unit/test_catrust.c`
fails the build when any certificate in the bundle is self-signed, is expired,
or cannot be chained to the system root store, and it checks that the
self-signed discriminator is not vacuous by running it over 50 real roots out
of the system store. Confirmed to fail, not assumed to:

    ./test_catrust /tmp/bad.pem        # a system root appended   -> SELF-SIGNED, rc=1
    ./test_catrust /tmp/stranger.pem   # an intermediate off our own CA
                                       #   -> "unable to get local issuer certificate", rc=1
    ./test_catrust /tmp/old.pem        # the same, backdated      -> EXPIRED, rc=1

### What it recovered, measured through the engine itself

`http_request()` against each host's root, same binary, bundle off and on:

    host                     without        with
    app.legco.gov.hk         rc=1 status=0  rc=0 status=200  11,150 bytes
    www.ansd.sn              rc=1 status=0  rc=0 status=200 560,592
    www.iwabuchi.co.jp       rc=1 status=0  rc=0 status=200  15,147
    www.kanamic.net          rc=1 status=0  rc=0 status=200  82,998
    www.ons.dz               rc=1 status=0  rc=0 status=200  39,303
    www.pcrwr.gov.pk         rc=1 status=0  rc=0 status=200 231,590
    www.sanden.co.jp         rc=1 status=0  rc=0 status=200  66,372
    www.suzuden.co.jp        rc=1 status=0  rc=0 status=200  44,383
    www.traas.co.jp          rc=1 status=0  rc=0 status=200  92,469

Nine of nine, and the 24 candidates on them re-probed **24 of 24 PASS**.

### And a registered row recovered for free

`NI_MARENA_PUBLICACIONES` (Nicaragua's environment ministry) was one of six
REGISTERED rows this fix was not aimed at. `www.marena.gob.ni` turns out to
omit the same `Sectigo Public Server Authentication CA DV R36` that
`www.ons.dz` omits, so the bundle already carried its missing link:
`rc=1 status=0` → **HTTP 200, 203,652 bytes**, with no change to the row.
Two of the other five are a DIFFERENT fault and one is a third — see
`docs/OPEN_ITEMS_2026-10-20.md`, D1-ter.

## What was NOT this, of the original thirty

* **2 are a genuine SNI mismatch** — `dataverse.openscholar.org` and
  `nesstar.statssa.gov.za` serve a certificate for a different name. Nothing
  short of not checking the hostname reaches them, and that is not on offer.
* **1 was local egress**, this container's own tunnel closing mid-exchange —
  unmeasured from here rather than a verdict about the host.
* **1 was the Yamaguchi 409** already refuted on 2026-10-20.
* **2 re-point rather than need a certificate**: `www.ip.kyusan-u.ac.jp` is
  served by `www.kyusan-u.ac.jp` (200), and `matrix.kde.org` delegates its
  Matrix traffic to `kde.modular.im:443`, which its `*.modular.im` certificate
  covers.

So D1's "remove the 31" would have deleted 24 live publishers for a fault that
is nine servers' chain configuration and ours to work around.

## The lesson

**Read the TLS error, not the word "TLS".** "Unable to get local issuer
certificate", "certificate has expired", "hostname mismatch" and "self-signed
certificate in chain" are four different findings with four different remedies,
and only one of them is about a certificate we should not trust. This tree has
the same shape on record twice already in `tools/jo_tls.py`'s own history — a
cipher security level read as a verification failure — and it keeps costing live
sources because the message is alarming and the cause is mundane.

The corollary for the engine: it had no way to say any of this. A chain fault
arrives as `rc=-1` and one `collector-fetch-failure` record naming a transport
error, which is honest and is not actionable. Distinguishing the four cases in
that record is open item D1-bis.
