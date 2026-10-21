#!/usr/bin/env python3
"""THE TLS context every tool in this directory uses, for one measured reason.

Python's `ssl.create_default_context()` refuses a handshake that the ENGINE
completes, and the gates in this directory are what decide whether a row ships.
A probe that reports a source DEAD that the engine measurably handles is the
worse of the two failure directions — CLAUDE.md says so about
`probe_hp_batch.py` twice already (the anchor regex that could not match an
href carrying a fragment, and the array_path resolver that could not cross an
array), and this is the same shape at the transport layer.

Measured 2026-10-20 on `yamaguchi-opendata.jp`, which serves 24 registered CKAN
organisation rows:

    default context                 FAIL  TLSV1_ALERT_INSUFFICIENT_SECURITY
    DEFAULT@SECLEVEL=1, verify ON   HTTP 200, 48,741 bytes

and the engine, asked the same question, stores 102 records across 3 requests.
So the host is alive, the row works, and only the tool could not see it. The
page-2 sweep reported 27 rows this way, 24 of them that one host.

It also supplies the intermediates that nine upstreams OMIT from their own
handshake — `native/data/tls/extra-intermediates.pem`, the same bundle the engine
loads through `core/catrust.c`. Without them, a host whose leaf is signed by a
public CA answers "unable to get local issuer certificate" and reads as a dead
endpoint; 24 probed candidates were rejected that way. Every certificate in
that file is an intermediate, so it can complete a chain and cannot anchor one;
`core/catrust.h` has the argument in full.

CERTIFICATE VERIFICATION STAYS ON. This lowers OpenSSL's cipher *security
level* so an older server's handshake is acceptable, and adds the middle of
some chains; it does not skip verification, and it must not be changed to.

What still fails after this fails for the engine too, and for three DIFFERENT
reasons — read the error, not the word "TLS" (measured 2026-10-21):

    api.ioda.inetintel.cc.gatech.edu   error 20 at depth 1 — the INTERMEDIATE's
    dataverse.ucla.edu                 issuer is an emSign root this store does
                                       not hold. Fixing it would mean a new
                                       trust ANCHOR, which is not on offer.
    microdata.nsonepal.gov.np          error 10 at depth 0 — the leaf expired on
                                       2026-10-16. The operator's to renew.
    dataverse.openscholar.org          the certificate is for another name; only
    nesstar.statssa.gov.za             not checking the hostname would get in.

Each of those answers `rc=-1` with a collector-fetch-failure record, which is
the honest outcome rather than a tool artefact.
"""
import os
import ssl

_CA = "/root/.ccr/ca-bundle.crt"
_EXTRA_REL = "../data/tls/extra-intermediates.pem"


def extra_intermediates_path():
    """The bundle core/catrust.c resolves, resolved the same way."""
    e = os.environ.get("JO_CA_EXTRA")
    if e:
        return e
    r = os.environ.get("JO_REPO_ROOT")
    if r:
        return os.path.join(r, "native/data/tls/extra-intermediates.pem")
    return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        _EXTRA_REL))


def context():
    """A verifying TLS context that reaches what the engine reaches."""
    ctx = ssl.create_default_context()
    if os.path.exists(_CA):
        try:
            ctx.load_verify_locations(_CA)
        except Exception:
            pass
    extra = extra_intermediates_path()
    if os.path.exists(extra):
        try:
            ctx.load_verify_locations(extra)
        except Exception:
            pass
    try:
        ctx.set_ciphers("DEFAULT@SECLEVEL=1")
    except Exception:
        # An OpenSSL build without security levels: the default is already
        # permissive enough, so there is nothing to relax.
        pass
    assert ctx.verify_mode == ssl.CERT_REQUIRED, "verification must stay on"
    assert ctx.check_hostname, "hostname checking must stay on"
    return ctx


if __name__ == "__main__":
    c = context()
    print("verify_mode=%s check_hostname=%s extra=%s"
          % (c.verify_mode, c.check_hostname, extra_intermediates_path()))
