/* core/clientip.h — which address a request came from, when a reverse proxy
 * stands in front of us.
 *
 * Every per-IP limit and every audit row's `ip` used the socket's peer. Behind
 * nginx, a load balancer or a tunnel that peer is the PROXY for every request,
 * so the break-glass TOTP limit (5 a minute) and the isochrone limit (12 a
 * minute) were one bucket shared by every user, and the audit chain recorded
 * the proxy as the source of every break-glass use (audit round two's last
 * open item, closed 2026-11-24).
 *
 * The fix must not let a client CHOOSE its own address: X-Forwarded-For is a
 * header anyone can send, so a server that believes it from any peer hands each
 * request a fresh bucket and writes whatever the caller likes into the audit
 * log. So it is believed only from a proxy the operator NAMED:
 *
 *   JO_TRUSTED_PROXIES  comma-separated addresses or CIDR blocks, IPv4 or IPv6
 *                       (e.g. "127.0.0.1,10.0.0.0/8,::1"). Unset or empty: the
 *                       peer is the client, exactly as before.
 *
 * With it set and the peer in the list, the X-Forwarded-For chain is read from
 * the RIGHT — the end the nearest proxy appended — skipping entries that are
 * themselves trusted proxies, and the first untrusted address is the client.
 * Reading from the left would take the address the client wrote itself. An
 * entry that does not parse as an address stops the walk at the last trusted
 * hop, so a malformed header can only put a request in its proxy's bucket (the
 * old behaviour), never in one of its choosing. */
#ifndef JO_CLIENTIP_H
#define JO_CLIENTIP_H

#include <stddef.h>

/* `peer` is the socket's address as text; `xff` is every X-Forwarded-For
 * header value joined with commas, or NULL. Writes the client address
 * (normalised by inet_ntop when it parses) to `out`. Returns 1 when the
 * address came from the header, 0 when it is the peer. */
int clientip_resolve(const char *peer, const char *xff, char *out, size_t n);

/* Re-read JO_TRUSTED_PROXIES (tests; the server reads it once). Returns the
 * number of entries accepted; an entry that does not parse is reported on
 * stderr and skipped, never widened into "trust everything". */
int clientip_reload(void);

#endif
