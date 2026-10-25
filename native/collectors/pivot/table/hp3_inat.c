/* collectors/pivot/table/hp3_inat.c -- iNaturalist's three observation feeds,
 * read from the live head without re-reading it.
 *
 * These were generated VJSON rows (collectors/feed/generated/vsrc_science_2.c)
 * asking for `order_by=created_at` and letting the walk advance `page`. The
 * registry health sweep read all three COLLISION — 375 of 4,000, 200 of 4,000
 * and 116 of 2,000 — and the cause is the hazard CLAUDE.md names rather than a
 * key: **a descending order over a live head drifts under the walk.** Measured
 * 2026-10-24: page 1 read twice a second apart is byte-identical, and page 1
 * against page 2 overlaps by **10 of 200**. iNaturalist takes on the order of
 * 4,000 observations an hour, so by the time the walk reaches page 20 the head
 * has moved and the boundaries have shifted. A record served twice is the
 * visible half; the invisible half is a record that no page served at all.
 *
 * The API's own answer is a KEYSET cursor, which is why it publishes one:
 * `order_by=id&order=desc` with `id_below=<the last id of the previous page>`.
 * Measured on the same beat — 200 ids strictly descending, then `id_below` of
 * that page's last id returning 200 more whose maximum is exactly one below it,
 * **overlap 0** — and the control `frobnicate=7` returns the identical set, so
 * the cursor is doing the work rather than any added parameter moving the
 * answer (CLAUDE.md, batch 55).
 *
 * `page_resume` is deliberately NOT used, and that is the whole design of these
 * three rows. A resumed keyset would march backwards through 385,217,394
 * observations and never come back to the head, which is exactly what the
 * cycling rules exclude for a row whose purpose is recency: each of these runs
 * hourly or three-hourly and exists to carry what was just recorded. So every
 * run starts at the head again and walks 20 pages DOWN from it — about the last
 * hour of observations, drift-free — and the next run starts from the new head.
 * An observation seen by two runs updates its own row, because the uid is the
 * observation id.
 *
 * The remainder is disclosed rather than implied: the envelope's
 * `total_results` is 385,217,394, so each run files a truncation notice naming
 * what it read against what exists. That is honest for a recency window and is
 * the convention this tree already follows for a corpus larger than anything
 * wants (CLAUDE.md: Spaceflight News, the STAC catalogues).
 *
 * Identity is `id`, the observation id — monotonic, assigned at creation, and
 * what `uri` is built from. Coordinates come from `location` ("lat,lon" in one
 * field, hpengine's latlon_key), so the geo these rows existed for survives the
 * move. */
#include "lib/hpengine.h"

static const hp_source HP3_INAT[] = {
  { .id = "geo-inaturalist-obs",
    .name = "iNaturalist - recent georeferenced observations",
    .name_ja = "iNaturalist - 位置情報付き最新観測",
    .category = "biodiversity", .portal = "https://www.inaturalist.org",
    .record_type = "species-observation",
    .tags = "\"inaturalist\",\"citizen\",\"observation\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://api.inaturalist.org/v1/observations?per_page=200&geo=true&order_by=id&order=desc",
    .array_path = "results",
    .next_path = "results.$last.id",
    .next_tmpl = "https://api.inaturalist.org/v1/observations?per_page=200&geo=true&order_by=id&order=desc&id_below={v}",
    .page_max = 20,   /* exhaustive-ok: a recency window, ~4,000 newest observations a run, with the upstream's own total disclosed */
    .id_keys = "id",
    .title_keys = "species_guess,place_guess",
    .date_keys = "created_at",
    .latlon_key = "location",
    .link_keys = "uri",
    .interval = 3600,
    .description = "Live citizen-science species observations, each carrying a "
      "coordinate pair. Read from the head of the feed down a keyset cursor "
      "(id_below), so the ~4,000 newest observations a run arrive without the "
      "page drift an offset walk over a live head suffers; the upstream's own "
      "count of what exists is disclosed per run" },

  { .id = "geo-inat-observations-japan",
    .name = "iNaturalist - Japan observations",
    .name_ja = "iNaturalist - 日本の観測",
    .category = "biodiversity", .portal = "https://www.inaturalist.org",
    .record_type = "species-observation",
    .tags = "\"inaturalist\",\"japan\",\"observation\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://api.inaturalist.org/v1/observations?per_page=200&geo=true&order_by=id&order=desc&place_id=6737",
    .array_path = "results",
    .next_path = "results.$last.id",
    .next_tmpl = "https://api.inaturalist.org/v1/observations?per_page=200&geo=true&order_by=id&order=desc&place_id=6737&id_below={v}",
    .page_max = 20,   /* exhaustive-ok: a recency window over one place, disclosed against the upstream's total */
    .id_keys = "id",
    .title_keys = "species_guess,place_guess",
    .date_keys = "created_at",
    .latlon_key = "location",
    .link_keys = "uri",
    .interval = 10800,
    .description = "Species observations recorded within Japan (place_id 6737), "
      "newest first down a keyset cursor" },

    /* per_page=100, not 200: the VJSON row this replaces read 100 a page and
     * these records are large (67 top-level fields including the whole taxon
     * tree). Measured 2026-10-25: 200 a page is 205 MB and 143 s a run against
     * an hourly cadence, so the move keeps the payload the row already had
     * rather than doubling it as a side effect of fixing the drift. */
  { .id = "geo-inat-research-small",
    .name = "iNaturalist - research-grade observations",
    .name_ja = "iNaturalist - リサーチグレード観測",
    .category = "biodiversity", .portal = "https://www.inaturalist.org",
    .record_type = "species-observation",
    .tags = "\"inaturalist\",\"researchgrade\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://api.inaturalist.org/v1/observations?per_page=100&geo=true&order_by=id&order=desc&quality_grade=research",
    .array_path = "results",
    .next_path = "results.$last.id",
    .next_tmpl = "https://api.inaturalist.org/v1/observations?per_page=100&geo=true&order_by=id&order=desc&quality_grade=research&id_below={v}",
    .page_max = 20,   /* exhaustive-ok: a recency window over the research-grade subset, disclosed */
    .id_keys = "id",
    .title_keys = "species_guess,place_guess",
    .date_keys = "created_at",
    .latlon_key = "location",
    .link_keys = "uri",
    .interval = 3600,
    .description = "Observations that have reached research grade — community "
      "identification plus date and coordinates — newest first down a keyset "
      "cursor" },
};

HP_REGISTER_TABLE(HP3_INAT)
