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

CERTIFICATE VERIFICATION STAYS ON. This lowers OpenSSL's cipher *security
level* so an older server's handshake is acceptable; it does not skip
verification, and it must not be changed to. The three rows that still fail
after this (IODA, Dataverse UCLA, NADA Nepal) fail for the engine too, with
`rc=-1` and a collector-fetch-failure record naming it — which is the honest
outcome rather than a tool artefact.
"""
import os
import ssl

_CA = "/root/.ccr/ca-bundle.crt"


def context():
    """A verifying TLS context that reaches what the engine reaches."""
    ctx = ssl.create_default_context()
    if os.path.exists(_CA):
        try:
            ctx.load_verify_locations(_CA)
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
    print("verify_mode=%s check_hostname=%s" % (c.verify_mode, c.check_hostname))
