/* core/layertab.h — the curated layer table (core/layers.def) + the
 * source→layer resolver behind /api/layers and /api/layers/:id/geojson.
 *
 * A "layer" groups sources by DATA TYPE and MODALITY (see layers.def for the
 * rule and why modality is declared, never inferred). Resolution assigns each
 * registered source to AT MOST ONE layer:
 *
 *   1. the first layers.def row (table order) with a matching term wins;
 *   2. else the source's own declared `.layer`, unless that id is in
 *      statusapi's STRIP set (a stripped declared layer would be invisible,
 *      and an invisible layer would make its rows unreachable — those
 *      sources fall through to 3 instead);
 *   3. else NO layer — the source's geocoded rows are served by the
 *      generated per-record_type catch-all (`rt-<slug>` /
 *      `unassigned-geocoded`), so nothing in intel_items is unreachable.
 *
 * Single assignment is what keeps a points layer and a heatmap layer from
 * ever sharing members, and what makes per-layer record counts sum exactly
 * to COUNT(*) WHERE lat IS NOT NULL. */
#ifndef JO_LAYERTAB_H
#define JO_LAYERTAB_H

#include <stddef.h>

typedef struct layer_row {
  const char *id;          /* layer id (also the /api/data/<id> key)        */
  const char *name;
  const char *category;
  const char *data_type;   /* what kind of data every member carries        */
  const char *modality;    /* point|heatmap|line|polygon|raster, or NULL =
                            * mixed/undecided (JSON null — never a guess)   */
  const char *match;       /* '|'-separated id:/idp:/layer:/cat: terms      */
} layer_row;

int              layertab_count(void);
const layer_row *layertab_at(int i);
const layer_row *layertab_get(const char *layer_id);   /* curated rows only */

/* Resolved layer id for a registered source (steps 1–2 above), or NULL when
 * the source is unassigned (step 3). Returned pointer aliases static
 * storage; valid for the process lifetime. */
const char *layertab_layer_for_source(const char *source_id);

/* malloc'd SQL fragment "'a','b','c'" of the member source ids of `layer_id`
 * (curated OR declared — any id some source resolves to). NULL when no
 * source resolves to it; *count (optional) gets the member count. Ids are
 * quote-escaped, so the fragment is safe to splice into an IN (...) list. */
char *layertab_members_in(const char *layer_id, int *count);

/* malloc'd SQL fragment of EVERY assigned source id — the complement is the
 * catch-all's population. NULL only when no source at all is assigned
 * (*count 0 then). */
char *layertab_assigned_in(int *count);

/* Generated-layer id slug for a record_type: lower-cased, [a-z0-9] runs kept,
 * everything else collapsed to '-'. "rt-" prefix is the CALLER's job. */
void layertab_rt_slug(const char *record_type, char *out, size_t cap);

#endif
