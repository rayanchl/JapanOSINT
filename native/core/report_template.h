/* core/report_template.{c,h} — the WRITER for tenant_report_templates.
 *
 * WHY THIS EXISTS. core/reportapi.c reads a workspace's report branding
 * (classification banner, header note, footer, logo) out of
 * tenant_report_templates, and its header calls the table "the seam, not the
 * feature": nothing in the tree wrote it (found 2026-11-12; the owner's
 * decision is "wire writers"). This is the feature:
 *
 *   GET /api/reports/template   any member of the active workspace
 *   PUT /api/reports/template   workspace owner/admin — the gate every other
 *                               workspace-settings write uses (tenantapi.c's
 *                               member routes) — audited "report_template.put"
 *
 * VALIDATION MIRRORS THE READER, so what is saved is exactly what a report
 * prints. Each field's byte bound is the size of the buffer report_template
 * (reportapi.h) copies it into, less the NUL: a longer value would be stored
 * and then silently cut by the reader's snprintf, which is a discard. So a
 * long value is a 400 naming the bound. classification and header_note are
 * one line (no control characters); footer_md may carry newlines and tabs.
 * There is no closed set of classifications: reportapi prints the banner text
 * verbatim and implies none ("e.g. CONFIDENTIAL // TLP:AMBER").
 *
 * logo_url must be https:// and is NEVER fetched by the server, at save or
 * at render: the HTML report links it as an <img src>, so the reader's
 * browser fetches it. It must also pass the same character rule reportapi's
 * safe_url() applies, or the renderer would silently drop it.
 *
 * An empty string or null clears a field; a PUT that leaves all four empty
 * deletes the row, so the workspace gets the plain unbranded document
 * (present = 0) rather than a "template" with nothing in it. */
#ifndef JO_REPORT_TEMPLATE_H
#define JO_REPORT_TEMPLATE_H

#include <stddef.h>
#include "db.h"
#include "tenantapi.h"

/* The route bodies; malloc'd JSON, *status set. NULL only on OOM. */
char *report_template_api_get(db_handle *db, const tenant_ctx *t, int *status);
char *report_template_api_put(db_handle *db, const tenant_ctx *t,
                              const char *body, size_t body_len, int *status);

#endif
