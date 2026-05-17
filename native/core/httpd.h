/* core/httpd.h — mongoose-based HTTP/1.1 + SSE server. Route table mirrors
 * index.js mounts: /api/health is pre-auth; all other /api/* pass the
 * auth_check gate (byte-parity with the Node 401/503 bodies); SSE is never
 * compressed (mongoose doesn't gzip), so it streams incrementally by
 * construction. */
#ifndef JO_HTTPD_H
#define JO_HTTPD_H
#include "db.h"
int httpd_serve(db_handle *db, int port);  /* blocks; returns on fatal error */
#endif
