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
CREATE TABLE IF NOT EXISTS gtfs_rt_alerts_fts_uid_map (
         uid   TEXT    NOT NULL PRIMARY KEY,
         rowid INTEGER NOT NULL
       ) WITHOUT ROWID;
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
CREATE TABLE IF NOT EXISTS tenant_idp_connections (
      tenant_id     TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      kind          TEXT NOT NULL CHECK(kind IN ('saml','scim')),
      provider_id   TEXT NOT NULL,        -- Jackson connection id
      status        TEXT NOT NULL DEFAULT 'pending',
      last_synced   TEXT,
      created_at    TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (tenant_id, kind)
    );
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
