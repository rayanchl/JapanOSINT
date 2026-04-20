// GTFS ingest — accepts an in-memory feed zip, parses routes, trips,
// stop_times, shapes, and calendar, and writes them into SQLite in one
// transaction. Idempotent per (org_id, feed_id).

import AdmZip from 'adm-zip';
import db from './database.js';

// Parse HH:MM:SS into seconds-since-midnight. GTFS permits H >= 24 to
// represent trips that cross midnight — we return the literal second count,
// so 25:15:00 = 90_900 (1 h 15 m into the next service day).
export function parseGtfsTime(s) {
  if (typeof s !== 'string') return null;
  const m = /^(\d{1,3}):(\d{2}):(\d{2})$/.exec(s.trim());
  if (!m) return null;
  return Number(m[1]) * 3600 + Number(m[2]) * 60 + Number(m[3]);
}

// Split one CSV line respecting double-quoted fields (GTFS stop names
// occasionally contain commas inside quotes).
export function splitCsvLine(line) {
  const out = [];
  let cur = '';
  let quoted = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '"') {
      quoted = !quoted;
    } else if (c === ',' && !quoted) {
      out.push(cur);
      cur = '';
    } else {
      cur += c;
    }
  }
  out.push(cur);
  return out;
}

export function parseCsv(text) {
  const lines = text.split(/\r?\n/).filter(Boolean);
  if (lines.length < 2) return { header: [], rows: [] };
  const header = splitCsvLine(lines[0]).map((s) => s.trim());
  const rows = [];
  for (let i = 1; i < lines.length; i++) {
    const cols = splitCsvLine(lines[i]);
    const row = {};
    for (let c = 0; c < header.length; c++) {
      row[header[c]] = (cols[c] ?? '').trim();
    }
    rows.push(row);
  }
  return { header, rows };
}

function numOrNull(v) {
  if (v == null || v === '') return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

/**
 * Ingest an in-memory GTFS zip buffer for one (orgId, feedId) pair. Writes
 * into SQLite inside one transaction. Re-running overwrites all rows for
 * that pair.
 *
 * @param {string} orgId
 * @param {string} feedId
 * @param {ArrayBuffer|Buffer} buffer
 * @returns {{routes:number, trips:number, stop_times:number, shapes:number, calendar:number}}
 */
export function ingestFeedZip(orgId, feedId, buffer) {
  const zipBuffer = Buffer.isBuffer(buffer) ? buffer : Buffer.from(buffer);
  const zip = new AdmZip(zipBuffer);
  const files = Object.fromEntries(
    zip.getEntries().map((e) => [e.entryName.toLowerCase(), e.getData().toString('utf8')]),
  );

  const counts = { routes: 0, trips: 0, stop_times: 0, shapes: 0, calendar: 0 };

  const tx = db.transaction(() => {
    for (const t of ['gtfs_routes', 'gtfs_trips', 'gtfs_stop_times', 'gtfs_shapes', 'gtfs_calendar']) {
      db.prepare(`DELETE FROM ${t} WHERE org_id = ? AND feed_id = ?`).run(orgId, feedId);
    }

    if (files['routes.txt']) {
      const { rows } = parseCsv(files['routes.txt']);
      const stmt = db.prepare(`INSERT INTO gtfs_routes
        (org_id, feed_id, route_id, short_name, long_name, route_type, color, text_color)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)`);
      for (const r of rows) {
        if (!r.route_id) continue;
        stmt.run(
          orgId, feedId, r.route_id,
          r.route_short_name || null,
          r.route_long_name || null,
          numOrNull(r.route_type),
          r.route_color || null,
          r.route_text_color || null,
        );
        counts.routes++;
      }
    }

    if (files['trips.txt']) {
      const { rows } = parseCsv(files['trips.txt']);
      const stmt = db.prepare(`INSERT INTO gtfs_trips
        (org_id, feed_id, trip_id, route_id, service_id, shape_id, headsign, direction_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)`);
      for (const r of rows) {
        if (!r.trip_id) continue;
        stmt.run(
          orgId, feedId, r.trip_id,
          r.route_id || null,
          r.service_id || null,
          r.shape_id || null,
          r.trip_headsign || null,
          numOrNull(r.direction_id),
        );
        counts.trips++;
      }
    }

    if (files['stop_times.txt']) {
      const { rows } = parseCsv(files['stop_times.txt']);
      const stmt = db.prepare(`INSERT INTO gtfs_stop_times
        (org_id, feed_id, trip_id, stop_sequence, stop_id, arrival_sec, departure_sec, shape_dist_traveled)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)`);
      for (const r of rows) {
        if (!r.trip_id) continue;
        const seq = numOrNull(r.stop_sequence);
        if (seq == null) continue;
        stmt.run(
          orgId, feedId, r.trip_id,
          seq,
          r.stop_id || null,
          parseGtfsTime(r.arrival_time),
          parseGtfsTime(r.departure_time),
          numOrNull(r.shape_dist_traveled),
        );
        counts.stop_times++;
      }
    }

    if (files['shapes.txt']) {
      const { rows } = parseCsv(files['shapes.txt']);
      const stmt = db.prepare(`INSERT INTO gtfs_shapes
        (org_id, feed_id, shape_id, seq, lat, lon, dist_m)
        VALUES (?, ?, ?, ?, ?, ?, ?)`);
      for (const r of rows) {
        if (!r.shape_id) continue;
        const seq = numOrNull(r.shape_pt_sequence);
        const lat = numOrNull(r.shape_pt_lat);
        const lon = numOrNull(r.shape_pt_lon);
        if (seq == null || lat == null || lon == null) continue;
        stmt.run(
          orgId, feedId, r.shape_id,
          seq, lat, lon,
          numOrNull(r.shape_dist_traveled),
        );
        counts.shapes++;
      }
    }

    if (files['calendar.txt']) {
      const { rows } = parseCsv(files['calendar.txt']);
      const stmt = db.prepare(`INSERT INTO gtfs_calendar
        (org_id, feed_id, service_id, mon, tue, wed, thu, fri, sat, sun, start_date, end_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`);
      for (const r of rows) {
        if (!r.service_id) continue;
        stmt.run(
          orgId, feedId, r.service_id,
          numOrNull(r.monday) || 0,
          numOrNull(r.tuesday) || 0,
          numOrNull(r.wednesday) || 0,
          numOrNull(r.thursday) || 0,
          numOrNull(r.friday) || 0,
          numOrNull(r.saturday) || 0,
          numOrNull(r.sunday) || 0,
          r.start_date || null,
          r.end_date || null,
        );
        counts.calendar++;
      }
    }
  });
  tx();
  return counts;
}
