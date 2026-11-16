/* core/routes.h — the HTTP route table.
 *
 * ONE row per route: which methods, which path pattern, which authorization
 * gate, and where it runs. core/httpd.c matches every request against this
 * table BEFORE it reaches the dispatch if-chain and enforces the row's gate in
 * one place (route_gate_enforce in httpd.c). A request under /api/ that matches
 * no row is refused 404 and never reaches the chain, so a matcher added to the
 * chain without a row here cannot be served; and because the gate is a field
 * of the row, "which door forgot its gate" is answered by reading this file
 * (or GET /api/admin/routes) instead of by auditing ~150 handlers.
 *
 * This file has no dependency on the HTTP server: routes_lookup() takes the
 * method and path as plain strings, so tests/unit/test_routes.c can exercise it
 * (and check the table against httpd.c's own matchers) without a socket.
 *
 * Pattern syntax
 *   "/api/health"            exact path
 *   "/api/alerts/*"          prefix: everything that starts with "/api/alerts/"
 *                            (a '*' may also follow a literal with no slash,
 *                            "/api/transit*", as the chain's starts() did)
 *   "/api/status/:id"        :name as the LAST element is exactly one non-empty
 *                            path segment
 *   "/api/sources/:id/detail" :name followed by a literal tail is everything
 *                            between the prefix and that tail, which is what
 *                            httpd.c's seg() has always accepted
 *
 * Lookup order is table order and the FIRST row whose pattern matches AND whose
 * method set contains the request's method wins. A row flagged RF_STRICT that
 * matches by pattern but not by method answers 405 at once instead of letting a
 * later, broader row (a prefix) take the request.
 */
#ifndef JO_ROUTES_H
#define JO_ROUTES_H
#include <stddef.h>

typedef enum {
  RG_PUBLIC = 0,      /* no credential at all (pre-auth)                      */
  RG_AUTH,            /* a valid bearer token                                 */
  RG_TENANT,          /* token + a resolvable workspace (X-Tenant-Id honoured) */
  RG_ROLE_ANALYST,    /* tenant role analyst, admin or owner (not viewer)     */
  RG_ROLE_ADMIN,      /* tenant role admin or owner                           */
  RG_RUN,             /* platform operator, or owner/admin of a TEAM (not
                       * personal) workspace: may run a collector            */
  RG_OPERATOR         /* platform operator (opgate_check)                     */
} route_gate;

typedef enum { RR_LOOP = 0, RR_WORKER } route_runs;

enum {
  RM_GET = 1, RM_POST = 2, RM_PUT = 4, RM_PATCH = 8, RM_DELETE = 16,
  RM_HEAD = 32,
  RM_ANY = 0x100      /* every method, including ones with no bit: the handler
                       * dispatches on the method itself                      */
};
enum { RF_STRICT = 1 };

typedef struct {
  unsigned    methods;      /* RM_* bits                                      */
  const char *pattern;
  route_gate  gate;
  route_runs  runs;
  unsigned    flags;        /* RF_*                                           */
  const char *name;         /* short stable name                              */
  const char *deny403;      /* RG_OPERATOR only: body of the 403 a handler has
                             * always sent for this route, NULL = the shared
                             * "Platform operator role required"              */
} route_row;

typedef enum { ROUTE_OK = 0, ROUTE_NOT_FOUND, ROUTE_METHOD_NOT_ALLOWED } route_status;

/* Match a request. `*out` is set on ROUTE_OK. */
route_status routes_lookup(const char *method, size_t mlen,
                           const char *uri, size_t ulen,
                           const route_row **out);

const route_row *routes_table(size_t *n);

const char *route_gate_name(route_gate g);
const char *route_runs_name(route_runs r);
/* "GET,POST" / "ANY"; returns buf. */
const char *routes_methods_str(unsigned methods, char *buf, size_t cap);

/* viewer 1, analyst 2, admin 3, owner 4, anything else 0 (fails closed). */
int routes_role_rank(const char *role);

/* The table as JSON for GET /api/admin/routes. malloc'd.
 * {"data":[{"name","method","path","gate","runs_on"}...],"meta":{"total":N,
 *  "by_gate":{...}}} */
char *routes_json(void);

/* Table self-consistency; 0 = ok, else a static description of the first
 * problem (a duplicate name, an empty pattern, a pattern with ':' in the
 * middle of a segment). Used by tests/unit/test_routes.c. */
const char *routes_selfcheck(void);

#endif
