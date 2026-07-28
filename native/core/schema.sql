CREATE TABLE IF NOT EXISTS _fts_meta (
    name         TEXT PRIMARY KEY,
    fingerprint  TEXT NOT NULL,
    columns      TEXT NOT NULL,
    tokenizer    TEXT NOT NULL,
    version      INTEGER NOT NULL,
    rebuilt_at   TEXT NOT NULL
  );
CREATE TABLE IF NOT EXISTS alert_events (
      id                       TEXT PRIMARY KEY,
      tenant_id                TEXT NOT NULL,
      rule_id                  TEXT NOT NULL,
      item_uid                 TEXT NOT NULL,
      matched_at               TEXT NOT NULL DEFAULT (datetime('now')),
      delivered_channels_json  TEXT NOT NULL DEFAULT '[]',
      suppressed               INTEGER NOT NULL DEFAULT 0,
      reason                   TEXT
    );
CREATE TABLE IF NOT EXISTS alert_rules (
      id                   TEXT PRIMARY KEY,
      tenant_id            TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      name                 TEXT NOT NULL,
      enabled              INTEGER NOT NULL DEFAULT 1,
      predicate_json       TEXT NOT NULL DEFAULT '{}',
      channels_json        TEXT NOT NULL DEFAULT '[]',
      dedup_window_sec     INTEGER NOT NULL DEFAULT 3600,
      storm_cap_per_hour   INTEGER NOT NULL DEFAULT 100,
      muted_until          TEXT,
      created_by           TEXT,
      created_at           TEXT NOT NULL DEFAULT (datetime('now')),
      updated_at           TEXT NOT NULL DEFAULT (datetime('now'))
    );
CREATE TABLE IF NOT EXISTS audit_events (
      id           TEXT PRIMARY KEY,
      tenant_id    TEXT NOT NULL,
      user_id      TEXT,
      action       TEXT NOT NULL,
      target       TEXT,
      payload_json TEXT,
      ts           TEXT NOT NULL DEFAULT (datetime('now')),
      ip           TEXT,
      ua           TEXT
    , prev_hash TEXT, row_hash TEXT, chain_seq INTEGER);
CREATE TABLE IF NOT EXISTS collector_anomaly (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id        TEXT NOT NULL REFERENCES sources(id),
    fetch_log_id     INTEGER REFERENCES fetch_log(id),
    verdict          TEXT NOT NULL CHECK(verdict IN ('records_drop','status_bad','sanity_failed','duration_outlier','manual')),
    reason           TEXT,
    evidence         TEXT,
    escalation_level INTEGER NOT NULL DEFAULT 0,
    created_at       TEXT NOT NULL DEFAULT (datetime('now')),
    resolved_at      TEXT,
    resolution       TEXT
  , triage_class TEXT, triage_confidence REAL, triage_evidence TEXT, triage_suggested_fix TEXT, triaged_at TEXT, triage_model TEXT);
CREATE TABLE IF NOT EXISTS collector_cache (
    key        TEXT PRIMARY KEY,
    fc_json    TEXT NOT NULL,
    fetched_at INTEGER NOT NULL,
    ttl_ms     INTEGER NOT NULL
  );
CREATE TABLE IF NOT EXISTS collector_repair (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    anomaly_id  INTEGER NOT NULL REFERENCES collector_anomaly(id),
    source_id   TEXT NOT NULL REFERENCES sources(id),
    status      TEXT NOT NULL CHECK(status IN ('verified','merged','rejected','needs_human','error')),
    action      TEXT,
    patch       TEXT,
    gate        TEXT,
    model       TEXT,
    pr_url      TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
  , triage_class TEXT);
CREATE TABLE IF NOT EXISTS collector_url_overrides (
    source_id  TEXT PRIMARY KEY REFERENCES sources(id),
    old_url    TEXT NOT NULL,
    new_url    TEXT NOT NULL,
    anomaly_id INTEGER,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
  );
CREATE TABLE IF NOT EXISTS collector_ttls (
    key        TEXT PRIMARY KEY,
    ttl_ms     INTEGER NOT NULL,
    source     TEXT NOT NULL,        -- 'registry' | 'default' | 'user'
    updated_at INTEGER NOT NULL
  );
CREATE TABLE IF NOT EXISTS entities (
      entity_id     TEXT PRIMARY KEY,
      type          TEXT NOT NULL,
      canonical     TEXT NOT NULL,
      norm_key      TEXT NOT NULL,
      name_ja       TEXT,
      name_romaji   TEXT,
      aliases_json  TEXT NOT NULL DEFAULT '[]',
      properties    TEXT NOT NULL DEFAULT '{}',
      mention_count INTEGER NOT NULL DEFAULT 0,
      first_seen_at TEXT NOT NULL DEFAULT (datetime('now')),
      last_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
      tenant_id     TEXT
    );
CREATE VIRTUAL TABLE IF NOT EXISTS entities_fts USING fts5(uid UNINDEXED, canonical, name_ja, name_romaji, keywords, tokenize='unicode61 remove_diacritics 1');
CREATE TABLE IF NOT EXISTS entities_fts_uid_map (
         uid   TEXT    NOT NULL PRIMARY KEY,
         rowid INTEGER NOT NULL
       ) WITHOUT ROWID;
CREATE TABLE IF NOT EXISTS entity_extraction_state (
      item_uid          TEXT PRIMARY KEY,
      extracted_at      TEXT NOT NULL DEFAULT (datetime('now')),
      extractor_version INTEGER NOT NULL DEFAULT 1,
      failed_count      INTEGER NOT NULL DEFAULT 0
    );
CREATE TABLE IF NOT EXISTS entity_mentions (
      entity_id   TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      item_uid    TEXT NOT NULL,
      source_id   TEXT NOT NULL,
      surface     TEXT,
      field       TEXT,
      confidence  REAL NOT NULL DEFAULT 0.5,
      extractor   TEXT NOT NULL,
      created_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (entity_id, item_uid, field)
    );
CREATE TABLE IF NOT EXISTS entity_merges (
      entity_a    TEXT NOT NULL,
      entity_b    TEXT NOT NULL,
      same        INTEGER NOT NULL CHECK(same IN (0,1)),
      confidence  REAL NOT NULL,
      reason      TEXT,
      decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (entity_a, entity_b)
    );
CREATE TABLE IF NOT EXISTS entity_relationships (
      src_entity_id TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      dst_entity_id TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      rel_type      TEXT NOT NULL,
      weight        REAL NOT NULL DEFAULT 1.0,
      evidence_uid  TEXT,
      first_seen_at TEXT NOT NULL DEFAULT (datetime('now')),
      last_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (src_entity_id, dst_entity_id, rel_type)
    );
CREATE TABLE IF NOT EXISTS fetch_log (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id       TEXT NOT NULL REFERENCES sources(id),
    timestamp       TEXT NOT NULL DEFAULT (datetime('now')),
    status          TEXT NOT NULL,
    records_fetched INTEGER DEFAULT 0,
    duration_ms     INTEGER,
    error           TEXT
  );
CREATE TABLE IF NOT EXISTS gtfs_calendar (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    service_id      TEXT NOT NULL,
    mon INTEGER, tue INTEGER, wed INTEGER, thu INTEGER,
    fri INTEGER, sat INTEGER, sun INTEGER,
    start_date      TEXT,
    end_date        TEXT,
    PRIMARY KEY (org_id, feed_id, service_id)
  );
CREATE TABLE IF NOT EXISTS gtfs_feeds (
    feed_id              TEXT PRIMARY KEY,
    ag_id                TEXT,
    ag_name              TEXT,
    pref_code            TEXT,
    pref_name            TEXT,
    feed_name            TEXT,
    fixed_current_url    TEXT,
    license_name         TEXT,
    license_url          TEXT,
    api_key_required     INTEGER NOT NULL DEFAULT 0,
    feed_end_date        TEXT,
    rt_catalog_url       TEXT,
    rt_api_key_required  INTEGER NOT NULL DEFAULT 0,
    rt_status            TEXT,
    last_refreshed_at    TEXT
  );
CREATE TABLE IF NOT EXISTS gtfs_operators (
    org_id          TEXT PRIMARY KEY,
    org_name        TEXT,
    hydrated_at     TEXT,
    feed_ids        TEXT NOT NULL DEFAULT '[]',
    stop_count      INTEGER NOT NULL DEFAULT 0,
    trip_count      INTEGER NOT NULL DEFAULT 0
  );
CREATE TABLE IF NOT EXISTS gtfs_routes (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    route_id        TEXT NOT NULL,
    short_name      TEXT,
    long_name       TEXT,
    route_type      INTEGER,
    color           TEXT,
    text_color      TEXT,
    PRIMARY KEY (org_id, feed_id, route_id)
  );
CREATE TABLE IF NOT EXISTS gtfs_rt_alerts (
    org_id           TEXT NOT NULL,
    alert_id         TEXT NOT NULL,
    route_ids        TEXT NOT NULL DEFAULT '[]',
    trip_ids         TEXT NOT NULL DEFAULT '[]',
    stop_ids         TEXT NOT NULL DEFAULT '[]',
    header_text      TEXT,
    description_text TEXT,
    cause            TEXT,
    effect           TEXT,
    reported_at      INTEGER NOT NULL,
    received_at      TEXT NOT NULL,
    PRIMARY KEY (org_id, alert_id)
  );
CREATE VIRTUAL TABLE IF NOT EXISTS gtfs_rt_alerts_fts USING fts5(uid UNINDEXED, header_text, description_text, tokenize='unicode61 remove_diacritics 1');
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS gtfs_rt_alerts_fts_uid_map (
         uid   TEXT    NOT NULL PRIMARY KEY,
         rowid INTEGER NOT NULL
       ) WITHOUT ROWID;
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS gtfs_rt_feeds (
    feed_id           TEXT PRIMARY KEY,
    ag_id             TEXT NOT NULL,
    ag_name           TEXT,
    rt_url            TEXT NOT NULL,
    poll_interval_s   INTEGER NOT NULL DEFAULT 30,
    last_polled_at    TEXT,
    last_ok_at        TEXT,
    last_status       TEXT,
    consecutive_fails INTEGER NOT NULL DEFAULT 0
  );
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS gtfs_rt_positions (
    org_id       TEXT NOT NULL,
    trip_id      TEXT NOT NULL,
    route_id     TEXT,
    lat          REAL NOT NULL,
    lon          REAL NOT NULL,
    bearing      REAL,
    speed_mps    REAL,
    reported_at  INTEGER NOT NULL,
    received_at  TEXT NOT NULL,
    PRIMARY KEY (org_id, trip_id)
  );
CREATE TABLE IF NOT EXISTS gtfs_rt_trip_updates (
    org_id             TEXT NOT NULL,
    trip_id            TEXT NOT NULL,
    route_id           TEXT,
    stop_id            TEXT,
    stop_sequence      INTEGER NOT NULL,
    arrival_delay_s    INTEGER,
    departure_delay_s  INTEGER,
    reported_at        INTEGER NOT NULL,
    received_at        TEXT NOT NULL,
    PRIMARY KEY (org_id, trip_id, stop_sequence)
  );
CREATE TABLE IF NOT EXISTS gtfs_shapes (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    shape_id        TEXT NOT NULL,
    seq             INTEGER NOT NULL,
    lat             REAL,
    lon             REAL,
    dist_m          REAL,
    PRIMARY KEY (org_id, feed_id, shape_id, seq)
  );
CREATE TABLE IF NOT EXISTS gtfs_stop_times (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    trip_id         TEXT NOT NULL,
    stop_sequence   INTEGER NOT NULL,
    stop_id         TEXT,
    arrival_sec     INTEGER,
    departure_sec   INTEGER,
    shape_dist_traveled REAL,
    PRIMARY KEY (org_id, feed_id, trip_id, stop_sequence)
  );
CREATE TABLE IF NOT EXISTS gtfs_stops (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    stop_id         TEXT NOT NULL,
    name            TEXT,
    lat             REAL,
    lon             REAL,
    PRIMARY KEY (org_id, feed_id, stop_id)
  );
CREATE TABLE IF NOT EXISTS gtfs_trips (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    trip_id         TEXT NOT NULL,
    route_id        TEXT,
    service_id      TEXT,
    shape_id        TEXT,
    headsign        TEXT,
    direction_id    INTEGER,
    PRIMARY KEY (org_id, feed_id, trip_id)
  );
CREATE TABLE IF NOT EXISTS intel_items (
    uid          TEXT PRIMARY KEY,
    source_id    TEXT NOT NULL,
    title        TEXT,
    body         TEXT,
    summary      TEXT,
    link         TEXT,
    author       TEXT,
    language     TEXT,
    published_at TEXT,
    fetched_at   TEXT NOT NULL,
    tags         TEXT,
    properties   TEXT NOT NULL DEFAULT '{}'
  , keywords TEXT, keywords_at TEXT, keywords_failed INTEGER DEFAULT 0, lat REAL, lon REAL, geom_source TEXT, geom_at TEXT, record_type TEXT, sub_source_id TEXT, geometry TEXT, geom_failed INTEGER DEFAULT 0, tenant_id TEXT NOT NULL DEFAULT 'legacy');
CREATE VIRTUAL TABLE IF NOT EXISTS intel_items_fts USING fts5(uid UNINDEXED, title, body, summary, keywords, tokenize='unicode61 remove_diacritics 1');
CREATE TABLE IF NOT EXISTS intel_items_fts_uid_map (
         uid   TEXT    NOT NULL PRIMARY KEY,
         rowid INTEGER NOT NULL
       ) WITHOUT ROWID;
-- Breach corpus materialized as searchable intel. Dedicated lean table + its own
-- FTS so billions of breach rows never dilute intel_items / operational search.
-- Populated by core/breach_store.c (eager --materialize ingest). Passwords are
-- hash-only (value IS NULL); plaintext secrets stay AES-GCM encrypted in the
-- $JO_BREACH_DIR shard corpus and are reachable only via the reveal=1 path.
CREATE TABLE IF NOT EXISTS breach_items (
    id         INTEGER PRIMARY KEY,
    keyid      TEXT NOT NULL UNIQUE,         -- "<type>:<sha1prefix>" (non-reversible)
    type       TEXT NOT NULL,               -- email | username | phone | password
    value      TEXT,                        -- cleartext identifier; NULL for passwords
    source_id  TEXT NOT NULL,
    hash       TEXT NOT NULL,               -- full SHA-1 hex (exact join to shard)
    has_secret INTEGER NOT NULL DEFAULT 0,
    count      INTEGER NOT NULL DEFAULT 1,
    breaches   TEXT,
    first_seen TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
  );
CREATE INDEX IF NOT EXISTS breach_items_type_idx ON breach_items(type);
-- Per-breach source lookups (drill-down + COUNT(*) GROUP BY source_id) once the
-- keyid is per-(identifier,breach). Backs the intel-source adapter (breach_adapter.c).
CREATE INDEX IF NOT EXISTS breach_items_source_idx ON breach_items(source_id, id);
CREATE VIRTUAL TABLE IF NOT EXISTS breach_fts USING fts5(
    value,
    content='breach_items', content_rowid='id',
    tokenize='unicode61 remove_diacritics 1'
  );
-- Breach catalog: one row per breach (e.g. "goose-creek"), surfaced as an intel
-- SOURCE via /api/intel/sources (category 'breach'). Loaded from the 1,018-entry
-- docs/breach-corpus.json manifest by core/breach_meta.c. breach_id == the slug
-- used as source_id on every breach_items row and entity_mentions.source_id.
CREATE TABLE IF NOT EXISTS breach_meta (
    breach_id        TEXT PRIMARY KEY,          -- slug == intel source id
    name             TEXT,
    title            TEXT,
    domain           TEXT,
    breach_date      TEXT,
    added_date       TEXT,
    pwn_count        INTEGER,
    data_classes_json TEXT,                     -- JSON array or NULL
    verified         INTEGER,
    sensitive        INTEGER,
    source_id        TEXT,                      -- manifest origin (e.g. hibp-corpus-seed)
    ingested_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
  );
CREATE TABLE IF NOT EXISTS llm_station_merges (
    uid_a       TEXT NOT NULL,
    uid_b       TEXT NOT NULL,
    same        INTEGER NOT NULL CHECK(same IN (0,1)),
    confidence  REAL NOT NULL,
    reason      TEXT,
    decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (uid_a, uid_b)
  );
CREATE TABLE IF NOT EXISTS memberships (
      user_id    TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
      tenant_id  TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      role       TEXT NOT NULL DEFAULT 'analyst'
                 CHECK(role IN ('owner','admin','analyst','viewer')),
      created_at TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (user_id, tenant_id)
    );
CREATE TABLE IF NOT EXISTS tenant_invites (
      id         TEXT PRIMARY KEY,
      tenant_id  TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      email      TEXT NOT NULL,                       -- lowercased
      role       TEXT NOT NULL DEFAULT 'analyst'
                 CHECK(role IN ('owner','admin','analyst','viewer')),
      invited_by TEXT,
      created_at TEXT NOT NULL DEFAULT (datetime('now')),
      UNIQUE(tenant_id, email)
    );
CREATE INDEX IF NOT EXISTS idx_tenant_invites_email
      ON tenant_invites(email);
CREATE TABLE IF NOT EXISTS odpt_station_timetable (
    station_id      TEXT NOT NULL,
    line_id         TEXT,
    calendar        TEXT,
    direction       TEXT,
    seq             INTEGER NOT NULL,
    departure_time  TEXT,
    destination_ja  TEXT,
    destination_en  TEXT,
    train_type      TEXT,
    train_name      TEXT,
    is_last         INTEGER NOT NULL DEFAULT 0,
    is_origin       INTEGER NOT NULL DEFAULT 0,
    org_id          TEXT,
    PRIMARY KEY (station_id, line_id, calendar, direction, seq)
  );
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS odpt_station_timetable_fetched (
    station_id   TEXT PRIMARY KEY,
    fetched_at   TEXT NOT NULL,
    entry_count  INTEGER NOT NULL DEFAULT 0
  );
CREATE TABLE IF NOT EXISTS sources (
        id            TEXT PRIMARY KEY,
        name          TEXT NOT NULL,
        type          TEXT NOT NULL CHECK(type IN ('api','dataset','scraped','web_request')),
        category      TEXT NOT NULL,
        url           TEXT,
        status        TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('online','offline','degraded','pending')),
        last_check    TEXT,
        last_success  TEXT,
        response_time_ms INTEGER,
        records_count INTEGER DEFAULT 0,
        error_message TEXT,
        probe_request_url     TEXT,
        probe_request_method  TEXT,
        probe_request_headers TEXT,
        probe_response_status INTEGER,
        probe_response_headers TEXT,
        probe_response_body   TEXT,
        probe_kind            TEXT
      , probe_consent INTEGER NOT NULL DEFAULT 0, quarantined_at TEXT, quarantined_until TEXT, quarantine_reason TEXT);
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS sso_group_role_map (
      tenant_id   TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      group_name  TEXT NOT NULL,
      role        TEXT NOT NULL CHECK(role IN ('owner','admin','analyst','viewer')),
      PRIMARY KEY (tenant_id, group_name)
    );
CREATE TABLE IF NOT EXISTS station_clusters (
    cluster_uid    TEXT PRIMARY KEY,
    name           TEXT NOT NULL,
    name_ja        TEXT,
    lat            REAL NOT NULL,
    lon            REAL NOT NULL,
    member_uids    TEXT NOT NULL,
    line_colors    TEXT,
    line_names     TEXT,
    line_refs      TEXT,
    line_modes     TEXT,
    mode_set       TEXT NOT NULL,
    operator_set   TEXT,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now'))
  );
CREATE TABLE IF NOT EXISTS station_footprints (
    footprint_id   TEXT PRIMARY KEY,
    cluster_uid    TEXT,
    name           TEXT,
    name_ja        TEXT,
    geometry       TEXT NOT NULL,
    bbox_min_lat   REAL NOT NULL,
    bbox_min_lon   REAL NOT NULL,
    bbox_max_lat   REAL NOT NULL,
    bbox_max_lon   REAL NOT NULL,
    source         TEXT,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now'))
  );
CREATE TABLE IF NOT EXISTS station_line_dots (
    cluster_uid  TEXT NOT NULL,
    way_uid      TEXT NOT NULL,
    line_color   TEXT NOT NULL,
    line_mode    TEXT NOT NULL,
    lon          REAL NOT NULL,
    lat          REAL NOT NULL,
    PRIMARY KEY (cluster_uid, way_uid)
  );
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS tenant_api_keys (
      id           TEXT PRIMARY KEY,
      tenant_id    TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      name         TEXT NOT NULL,
      hashed_key   TEXT NOT NULL UNIQUE,
      scopes_json  TEXT NOT NULL DEFAULT '[]',
      created_by   TEXT,
      created_at   TEXT NOT NULL DEFAULT (datetime('now')),
      last_used_at TEXT,
      revoked_at   TEXT
    );
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS tenant_idp_connections (
      tenant_id     TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      kind          TEXT NOT NULL CHECK(kind IN ('saml','scim')),
      provider_id   TEXT NOT NULL,        -- Jackson connection id
      status        TEXT NOT NULL DEFAULT 'pending',
      last_synced   TEXT,
      created_at    TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (tenant_id, kind)
    );
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
CREATE TABLE IF NOT EXISTS tenant_quotas (
      tenant_id       TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      source_id       TEXT NOT NULL,
      day             TEXT NOT NULL,
      platform_used   INTEGER NOT NULL DEFAULT 0,
      byok_used       INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY (tenant_id, source_id, day)
    );
CREATE TABLE IF NOT EXISTS tenant_secrets (
      tenant_id            TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      key_name             TEXT NOT NULL,
      encrypted_value      BLOB NOT NULL,
      fallback_to_platform INTEGER NOT NULL DEFAULT 1,
      created_by           TEXT,
      created_at           TEXT NOT NULL DEFAULT (datetime('now')),
      last_used_at         TEXT,
      PRIMARY KEY (tenant_id, key_name)
    );
CREATE TABLE IF NOT EXISTS tenants (
      id                  TEXT PRIMARY KEY,
      slug                TEXT UNIQUE NOT NULL,
      name                TEXT NOT NULL,
      plan                TEXT NOT NULL DEFAULT 'free'
                          CHECK(plan IN ('free','pro','team','enterprise')),
      stripe_customer_id  TEXT,
      require_sso         INTEGER NOT NULL DEFAULT 0,
      created_at          TEXT NOT NULL DEFAULT (datetime('now'))
    , key_write_policy TEXT NOT NULL DEFAULT 'owner_only'
       CHECK(key_write_policy IN ('owner_only','selected_member','all_members')), key_write_member_id TEXT);
CREATE TABLE IF NOT EXISTS users (
      id                 TEXT PRIMARY KEY,
      supabase_user_id   TEXT UNIQUE,
      email              TEXT NOT NULL,
      display_name       TEXT,
      created_at         TEXT NOT NULL DEFAULT (datetime('now'))
    );
CREATE INDEX IF NOT EXISTS idx_alert_events_rule_item
      ON alert_events(rule_id, item_uid);
CREATE INDEX IF NOT EXISTS idx_alert_events_rule_ts
      ON alert_events(rule_id, matched_at DESC);
CREATE INDEX IF NOT EXISTS idx_alert_rules_tenant_enabled
      ON alert_rules(tenant_id, enabled);
CREATE INDEX IF NOT EXISTS idx_anomaly_open
    ON collector_anomaly(source_id, created_at DESC) WHERE resolved_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_anomaly_unresolved
    ON collector_anomaly(created_at DESC) WHERE resolved_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_anomaly_untriaged
        ON collector_anomaly(created_at) WHERE resolved_at IS NULL AND triaged_at IS NULL;
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
-- (index over the unused tenant_api_keys table)
CREATE INDEX IF NOT EXISTS idx_apikeys_tenant ON tenant_api_keys(tenant_id);
CREATE INDEX IF NOT EXISTS idx_audit_chain
      ON audit_events(tenant_id, chain_seq)
      WHERE row_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_audit_tenant_ts
      ON audit_events(tenant_id, ts DESC);
CREATE INDEX IF NOT EXISTS idx_collector_cache_fetched
    ON collector_cache(fetched_at);
CREATE INDEX IF NOT EXISTS idx_em_entity ON entity_mentions(entity_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_em_item   ON entity_mentions(item_uid);
CREATE INDEX IF NOT EXISTS idx_em_source ON entity_mentions(source_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_entities_normkey
      ON entities(type, norm_key);
CREATE INDEX IF NOT EXISTS idx_entities_type_seen
      ON entities(type, last_seen_at DESC);
CREATE INDEX IF NOT EXISTS idx_er_dst ON entity_relationships(dst_entity_id, weight DESC);
CREATE INDEX IF NOT EXISTS idx_er_src ON entity_relationships(src_entity_id, weight DESC);
CREATE INDEX IF NOT EXISTS idx_gtfs_feeds_agency ON gtfs_feeds(ag_id);
CREATE INDEX IF NOT EXISTS idx_gtfs_rt_alerts_reported
    ON gtfs_rt_alerts(reported_at);
-- NOT IMPLEMENTED by the C server: nothing reads or writes this table.
-- (index over the unused gtfs_rt_positions table)
CREATE INDEX IF NOT EXISTS idx_gtfs_rt_positions_reported
    ON gtfs_rt_positions(reported_at);
CREATE INDEX IF NOT EXISTS idx_gtfs_rt_trip_updates_reported
    ON gtfs_rt_trip_updates(reported_at);
CREATE INDEX IF NOT EXISTS idx_gtfs_rt_trip_updates_stop
    ON gtfs_rt_trip_updates(org_id, stop_id);
CREATE INDEX IF NOT EXISTS idx_gtfs_stop_times_stop
    ON gtfs_stop_times(stop_id);
CREATE INDEX IF NOT EXISTS idx_gtfs_stop_times_trip_time
    ON gtfs_stop_times(org_id, feed_id, trip_id, departure_sec);
/* active-trips resolves service_id -> trips per feed on every request. */
CREATE INDEX IF NOT EXISTS idx_gtfs_trips_service
    ON gtfs_trips(org_id, feed_id, service_id);
CREATE INDEX IF NOT EXISTS idx_intel_items_fetched
    ON intel_items(fetched_at DESC);
CREATE INDEX IF NOT EXISTS idx_intel_items_geom
        ON intel_items(lat, lon) WHERE lat IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_intel_items_geom_pending
        ON intel_items(source_id) WHERE lat IS NULL AND geom_source IS NULL;
CREATE INDEX IF NOT EXISTS idx_intel_items_source
    ON intel_items(source_id, published_at DESC);
CREATE INDEX IF NOT EXISTS idx_intel_items_subsrc
        ON intel_items(source_id, sub_source_id);
CREATE INDEX IF NOT EXISTS idx_intel_items_tenant_fetched
      ON intel_items(tenant_id, fetched_at DESC);
CREATE INDEX IF NOT EXISTS idx_intel_items_tenant_source
      ON intel_items(tenant_id, source_id);
CREATE INDEX IF NOT EXISTS idx_intel_items_type
        ON intel_items(record_type, source_id);
CREATE INDEX IF NOT EXISTS idx_log_source ON fetch_log(source_id);
CREATE INDEX IF NOT EXISTS idx_log_ts     ON fetch_log(timestamp);
CREATE INDEX IF NOT EXISTS idx_memberships_tenant ON memberships(tenant_id);
CREATE INDEX IF NOT EXISTS idx_odpt_station_timetable_station
    ON odpt_station_timetable(station_id);
CREATE INDEX IF NOT EXISTS idx_repair_anomaly
    ON collector_repair(anomaly_id);
CREATE INDEX IF NOT EXISTS idx_repair_source
    ON collector_repair(source_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_station_clusters_ll ON station_clusters(lat, lon);
CREATE INDEX IF NOT EXISTS idx_station_footprints_bbox
    ON station_footprints(bbox_min_lat, bbox_min_lon, bbox_max_lat, bbox_max_lon);
CREATE INDEX IF NOT EXISTS idx_station_footprints_cluster
    ON station_footprints(cluster_uid);
CREATE INDEX IF NOT EXISTS idx_station_line_dots_cluster
    ON station_line_dots(cluster_uid);
CREATE INDEX IF NOT EXISTS idx_station_line_dots_mode
    ON station_line_dots(line_mode);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

-- ─────────────────────────────────────────────────────────────────────────────
-- Roadmap P0/P1/P2 additions. Everything below is IF NOT EXISTS, like the rest
-- of this generated file. NOTE: added COLUMNS on pre-existing tables are NOT
-- here — CREATE TABLE IF NOT EXISTS no-ops on an existing table, so a column
-- appended to a CREATE would never reach a deployed DB. Those live in
-- db.c's ensure_column() boot-migration block (alert_events.read_at,
-- entity_relationships.pmi/lift/co_count/stats_at).
-- ─────────────────────────────────────────────────────────────────────────────

-- P0.2 delivery ledger. delivered_channels_json alone cannot express
-- "tried 4 times, 502"; this table is what makes a failed alert diagnosable.
-- The UNIQUE index is load-bearing, not an optimization: it makes enqueue
-- idempotent so a crash or racing worker tick can never double-send.
CREATE TABLE IF NOT EXISTS alert_deliveries (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  event_id TEXT NOT NULL, channel_idx INTEGER NOT NULL,
  channel_type TEXT NOT NULL, target TEXT,
  attempt INTEGER NOT NULL DEFAULT 0,
  status TEXT NOT NULL,            -- pending|ok|failed|dead|skipped
  http_code INTEGER, error TEXT,
  next_attempt_at TEXT, attempted_at TEXT);
CREATE INDEX IF NOT EXISTS idx_alert_deliveries_due
  ON alert_deliveries(status, next_attempt_at);
CREATE INDEX IF NOT EXISTS idx_alert_deliveries_event
  ON alert_deliveries(event_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_alert_deliveries_uniq
  ON alert_deliveries(event_id, channel_idx);

-- Item 14 — cases: the investigation spine. case_items.snapshot_json holds a
-- denormalized display copy on purpose: scraped intel gets re-fetched and
-- breach corpora get re-ingested, and a pin that later renders as "deleted
-- item" is worthless in a report.
CREATE TABLE IF NOT EXISTS cases (
  id TEXT PRIMARY KEY,
  tenant_id TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
  name TEXT NOT NULL, summary TEXT,
  status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','closed','archived')),
  priority INTEGER NOT NULL DEFAULT 0,
  created_by TEXT, created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')), closed_at TEXT);
CREATE TABLE IF NOT EXISTS case_members (
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  role TEXT NOT NULL DEFAULT 'contributor' CHECK(role IN ('lead','contributor','viewer')),
  PRIMARY KEY (case_id, user_id));
CREATE TABLE IF NOT EXISTS case_items (
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
  label TEXT, snapshot_json TEXT,
  added_by TEXT, added_at TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (case_id, ref_type, ref_id));
CREATE TABLE IF NOT EXISTS case_activity (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  actor_id TEXT, kind TEXT NOT NULL,
  body TEXT, target_ref TEXT, mentions_json TEXT NOT NULL DEFAULT '[]',
  ts TEXT NOT NULL DEFAULT (datetime('now')));
CREATE INDEX IF NOT EXISTS idx_cases_tenant_status
  ON cases(tenant_id, status, updated_at DESC);
CREATE INDEX IF NOT EXISTS idx_cases_tenant_updated
  ON cases(tenant_id, updated_at DESC, id);
CREATE INDEX IF NOT EXISTS idx_case_items_ref ON case_items(ref_type, ref_id);
CREATE INDEX IF NOT EXISTS idx_case_items_case_added
  ON case_items(case_id, added_at DESC);
CREATE INDEX IF NOT EXISTS idx_case_members_user ON case_members(user_id);
CREATE INDEX IF NOT EXISTS idx_case_activity_case ON case_activity(case_id, ts DESC);

-- Item 15 — annotations. Soft-deleted only (deleted_at); analyst notes are
-- evidence and a hard DELETE would undermine the chain-of-custody work.
CREATE TABLE IF NOT EXISTS annotations (
  id TEXT PRIMARY KEY,
  tenant_id TEXT NOT NULL,
  case_id TEXT REFERENCES cases(id) ON DELETE CASCADE,
  ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
  body_md TEXT NOT NULL, author_id TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT, deleted_at TEXT);
CREATE INDEX IF NOT EXISTS idx_annotations_ref
  ON annotations(ref_type, ref_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_annotations_tenant
  ON annotations(tenant_id, created_at DESC);

-- Item 38 — saved searches + history. History is PER-USER, not per-tenant:
-- what an analyst is investigating is not something a tenant admin should be
-- able to read. The user predicate is enforced in savedsearchapi.c.
CREATE TABLE IF NOT EXISTS saved_searches (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
  name TEXT, kind TEXT NOT NULL CHECK(kind IN ('intel','osint','entity','breach','map')),
  params_json TEXT NOT NULL, pinned INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  last_run_at TEXT, run_count INTEGER NOT NULL DEFAULT 0);
CREATE INDEX IF NOT EXISTS idx_saved_searches_owner
  ON saved_searches(tenant_id, user_id, pinned DESC, created_at DESC);
CREATE TABLE IF NOT EXISTS search_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
  kind TEXT NOT NULL, params_json TEXT NOT NULL, result_count INTEGER,
  ts TEXT NOT NULL DEFAULT (datetime('now')));
CREATE INDEX IF NOT EXISTS idx_search_history_owner
  ON search_history(tenant_id, user_id, ts DESC);

-- Item 16 — optional per-tenant report branding (the paid-tier seam).
-- reportapi.c probes for this table and each column independently, so its
-- absence just yields an unbranded document.
CREATE TABLE IF NOT EXISTS tenant_report_templates (
  tenant_id TEXT PRIMARY KEY REFERENCES tenants(id) ON DELETE CASCADE,
  classification TEXT, header_note TEXT, footer_md TEXT, logo_url TEXT,
  updated_at TEXT NOT NULL DEFAULT (datetime('now')));

-- Item 24 — breach exposure monitoring. value_hash is SHA-1 of the normalized
-- identifier; the PLAINTEXT IS NEVER STORED, not here and not in audit rows —
-- a monitor list of customer emails is itself a high-value breach target.
CREATE TABLE IF NOT EXISTS breach_monitors (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL,
  kind TEXT NOT NULL CHECK(kind IN ('email','domain','username','phone')),
  value_hash TEXT NOT NULL,
  value_domain TEXT,
  label TEXT, rule_id TEXT, created_by TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now')), last_checked_at TEXT);
CREATE INDEX IF NOT EXISTS idx_breach_monitors_hash ON breach_monitors(value_hash);
CREATE INDEX IF NOT EXISTS idx_breach_monitors_domain ON breach_monitors(value_domain);
CREATE INDEX IF NOT EXISTS idx_breach_monitors_tenant ON breach_monitors(tenant_id);
CREATE INDEX IF NOT EXISTS idx_breach_monitors_tenant_created
  ON breach_monitors(tenant_id, created_at DESC, id);
-- NOT an optimization: breach_items had no index on `hash` at all (only type,
-- (source_id,id) and UNIQUE(keyid)), so every monitor check was a full scan of
-- a corpus measured in millions of rows.
CREATE INDEX IF NOT EXISTS idx_breach_items_hash ON breach_items(hash);
-- NOTE: the index over breach_items.value_domain is deliberately NOT here.
-- schema.sql is exec'd BEFORE db.c's ensure_column() block, so indexing a
-- column that does not exist yet would fail and abandon the rest of this
-- script. breach_monitor_migrate() creates the column and its index together.

-- Item 17 — evidence / chain of custody. Content-addressed blobs on disk,
-- rows hash-chained with the SAME prev_hash/row_hash/chain_seq scheme as
-- audit_events so /api/evidence/verify can re-walk them.
--
-- The reaper NEVER deletes rows: removing one breaks prev_hash for everything
-- after it and is indistinguishable from tampering. It unlinks blobs only;
-- the row survives as proof of what the hash was, and readers report a reaped
-- blob as 410, not 404.
--
-- No tenant_id by design: this records what platform collectors fetched,
-- which is server-wide. Raw reads are operator-gated (opgate_check), not
-- tenant-role-gated. Per-tenant evidence would be a schema change plus a
-- per-tenant chain, not a filter.
CREATE TABLE IF NOT EXISTS evidence (
  id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, source_id TEXT NOT NULL,
  captured_at TEXT NOT NULL DEFAULT (datetime('now')),
  request_url TEXT, request_method TEXT, request_headers TEXT,
  response_status INTEGER, response_headers TEXT,
  content_sha256 TEXT NOT NULL, content_bytes INTEGER, content_type TEXT,
  blob_path TEXT NOT NULL,
  prev_hash TEXT, row_hash TEXT, chain_seq INTEGER);
CREATE INDEX IF NOT EXISTS idx_evidence_item ON evidence(item_uid, captured_at DESC);
-- Load-bearing: the insert is ON CONFLICT(content_sha256,item_uid) DO NOTHING.
CREATE UNIQUE INDEX IF NOT EXISTS idx_evidence_sha ON evidence(content_sha256, item_uid);

-- Items 9/21 — geofence AOIs + entity watchlists.
-- updated_at is load-bearing on both: alert_eval's rule cache keys its
-- generation off it, so editing a shape invalidates every rule that
-- geofences on it. Without it a cached rule keeps using the old geometry.
CREATE TABLE IF NOT EXISTS areas_of_interest (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
  name TEXT NOT NULL, kind TEXT NOT NULL CHECK(kind IN ('bbox','polygon','circle')),
  geometry_json TEXT NOT NULL,
  bbox_w REAL, bbox_s REAL, bbox_e REAL, bbox_n REAL,
  created_by TEXT, created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')));
CREATE INDEX IF NOT EXISTS idx_aoi_tenant ON areas_of_interest(tenant_id, created_at DESC);

CREATE TABLE IF NOT EXISTS watchlists (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, name TEXT NOT NULL,
  entity_ids_json TEXT NOT NULL DEFAULT '[]', rule_id TEXT, created_by TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')));
CREATE INDEX IF NOT EXISTS idx_watchlists_tenant ON watchlists(tenant_id, created_at DESC);

-- Resumable watermark for the entity-mention reconciling sweep. Entity
-- extraction runs asynchronously long after ingest, so a watchlist evaluated
-- only at emit() time would never fire; the sweep re-evaluates items whose
-- mentions were written since the last watermark.
CREATE TABLE IF NOT EXISTS alert_eval_cursor (
  k TEXT PRIMARY KEY, v TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT (datetime('now')));

-- Required by that sweep: idx_em_entity is (entity_id, created_at DESC) and
-- cannot serve a bare range scan over created_at.
CREATE INDEX IF NOT EXISTS idx_em_created ON entity_mentions(created_at);

-- Item 26 — content change detection for scraped sources. Distinct from
-- maint_detect.c, which detects collector FAILURE; this detects the page
-- actually changing, which is often the intel itself.
-- The hash is over NORMALIZED extracted text (timestamps, nonces, counters
-- and cache-busters stripped) — without that every page "changes" on every
-- fetch and the feature becomes a noise generator.
CREATE TABLE IF NOT EXISTS content_snapshots (
  source_id TEXT NOT NULL, ref_key TEXT NOT NULL,
  content_sha256 TEXT NOT NULL, extract_text TEXT,
  captured_at TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (source_id, ref_key, content_sha256));
CREATE INDEX IF NOT EXISTS idx_content_snapshots_recent
  ON content_snapshots(source_id, ref_key, captured_at DESC);

-- Item 27 — media assets (EXIF / pHash / OCR). EXIF is the part that works
-- with no dependencies: it is byte-level APP1/TIFF parsing, no pixel decode.
-- EXIF GPS is genuine intel — it geolocates items that have no coordinates.
-- pHash and OCR are optional external-tool seams and stay NULL when the tool
-- is absent (there is no image decoder linked into this binary).
CREATE TABLE IF NOT EXISTS media_assets (
  id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, url TEXT NOT NULL,
  sha256 TEXT, bytes INTEGER, content_type TEXT,
  phash INTEGER, exif_json TEXT, ocr_text TEXT, ocr_conf REAL,
  width INTEGER, height INTEGER,
  analyzed_at TEXT, analyze_failed INTEGER NOT NULL DEFAULT 0,
  UNIQUE(item_uid, url));
CREATE INDEX IF NOT EXISTS idx_media_assets_item ON media_assets(item_uid);
CREATE INDEX IF NOT EXISTS idx_media_assets_sha ON media_assets(sha256);
CREATE INDEX IF NOT EXISTS idx_media_assets_pending
  ON media_assets(analyze_failed)
  WHERE analyzed_at IS NULL AND analyze_failed < 3;
CREATE INDEX IF NOT EXISTS idx_media_assets_exif
  ON media_assets(item_uid) WHERE exif_json IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_media_assets_ocr
  ON media_assets(item_uid) WHERE ocr_text IS NOT NULL;
CREATE TABLE IF NOT EXISTS media_state (k TEXT PRIMARY KEY, v TEXT NOT NULL);

-- Item 28 — scheduled camera stills. Turns a camera from a live-only view
-- into a diffable time series. Per-camera opt-in, default OFF: one frame per
-- camera per interval forever is the fastest way to fill a disk in this
-- roadmap, so retention is enforced on write AND by a sweep pod.
CREATE TABLE IF NOT EXISTS camera_stills (
  id TEXT PRIMARY KEY, camera_id TEXT NOT NULL,
  captured_at TEXT NOT NULL DEFAULT (datetime('now')),
  sha256 TEXT NOT NULL, blob_path TEXT, bytes INTEGER,
  phash INTEGER, ocr_text TEXT, width INTEGER, height INTEGER,
  scene_changed INTEGER);
CREATE INDEX IF NOT EXISTS idx_camera_stills_cam
  ON camera_stills(camera_id, captured_at DESC);
-- Per-camera failure/backoff state, so one dead camera cannot stall the rest.
CREATE TABLE IF NOT EXISTS camera_stills_state (
  camera_id TEXT PRIMARY KEY, fail_count INTEGER NOT NULL DEFAULT 0,
  last_attempt_at TEXT, next_attempt_at TEXT, last_ok_at TEXT,
  last_error TEXT, last_kind TEXT);

-- Chunked upload staging for case attachments. Parts land on disk under
-- $JO_UPLOAD_DIR, never in SQLite: a 64 MiB blob in a row would wreck the
-- page cache of a multi-GB database. This table is metadata only.
--
-- Why chunking at all: MG_MAX_RECV_SIZE is 3 MB and mongoose buffers a whole
-- request body in the event loop, so a single-shot upload would either cap
-- attachments at a few MB or raise the per-connection memory ceiling for
-- EVERY connection in the server.
CREATE TABLE IF NOT EXISTS uploads (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT,
  filename TEXT, content_type TEXT, total_bytes INTEGER,
  received_bytes INTEGER NOT NULL DEFAULT 0,
  part_count INTEGER NOT NULL DEFAULT 0,
  sha256 TEXT, status TEXT NOT NULL DEFAULT 'open',   -- open|committed|aborted
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  committed_at TEXT);
-- Carries no bytes: only (seq, size, arrival). It buys idempotent retries via
-- ON CONFLICT and lets received_bytes be RECOMPUTED with SUM() after every
-- part rather than incremented, so a re-sent part can never drift the total.
CREATE TABLE IF NOT EXISTS upload_parts (
  upload_id TEXT NOT NULL, seq INTEGER NOT NULL,
  bytes INTEGER NOT NULL DEFAULT 0,
  received_at TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (upload_id, seq));
CREATE INDEX IF NOT EXISTS idx_uploads_tenant_status
  ON uploads(tenant_id, status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_uploads_status_created ON uploads(status, created_at);
CREATE INDEX IF NOT EXISTS idx_upload_parts_upload ON upload_parts(upload_id, seq);
