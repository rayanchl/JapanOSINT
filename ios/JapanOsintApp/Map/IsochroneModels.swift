import Foundation
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// GTFS travel-time reachability (roadmap 32) — the client side of
// `GET /api/isochrone` (core/isochrone.{c,h}).
//
// The response is a GeoJSON FeatureCollection with a FOREIGN `meta` member —
// note `meta`, not the `_meta` that `FeatureCollection` in Models.swift reads.
// That one difference is why this has its own decoder rather than reusing the
// layer decoder: pointed at this payload, `FeatureCollection` would silently
// yield `meta == nil` and every honesty signal below would vanish.
//
// WHAT MUST SURVIVE THE DECODE, and why each matters when it is shown:
//   • `note`      — the server says in-band when the answer is degenerate ("no
//                   gtfs_stops within reach", or "no scheduled service departs
//                   within the budget … the polygon shown is walking reach
//                   only"). A walking-only blob looks exactly like a transit
//                   isochrone on a map. Dropping the note turns an honest
//                   result into a misleading one, so it is non-optional in the
//                   UI: `IsochroneOverlay` renders it whenever present.
//   • `truncated` — the search hit an internal bound, so the polygon UNDERSTATES
//                   reach.
//   • `cached`    — the answer may be up to ISO_CACHE_TTL_MS old.
//   • `stopsFirstReachedByTransit` — 0 is the machine-readable form of the
//                   walking-only case above.
//
// Bands arrive LARGEST FIRST (isochrone.h), which is already correct paint
// order: wide band underneath, tight band on top. `bands` preserves server
// order deliberately — do not sort it.
// ─────────────────────────────────────────────────────────────────────────────

/// One travel-time band: everywhere reachable within `bandMin` minutes.
struct IsochroneBand: Identifiable {
    /// Minutes threshold this polygon bounds.
    let bandMin: Int
    /// GTFS stops that fell inside the threshold.
    let stopsWithin: Int
    /// Always a MultiPolygon from the server; typed as `Geometry` so it can go
    /// straight into the same map plumbing layers use.
    let geometry: Geometry

    var id: Int { bandMin }

    /// "20 min" — the band label used on the map and in the legend.
    var label: String { "\(bandMin) min" }
}

/// Grid the contour was sampled on. Surfaced because cell size is the
/// resolution limit of the polygon — a 150 m cell cannot express a 40 m alley.
struct IsochroneGrid: Decodable, Hashable {
    let cell_m: Double?
    let nx: Int?
    let ny: Int?
}

/// Echo of the parameters the server actually used, AFTER its own clamping.
/// Displayed rather than the values requested: the server clamps `max_min` to
/// 5…90, `max_walk_m` to 100…2000 and so on, and showing what was asked for
/// when something else was computed is the same class of lie as a fabricated
/// row.
struct IsochroneParams: Decodable, Hashable {
    let max_min: Int?
    let max_walk_m: Double?
    let walk_kmh: Double?
    let transfer_radius_m: Double?
    let max_transfers: Int?
    let bands: [Int]?
}

struct IsochroneOrigin: Decodable, Hashable {
    let lat: Double
    let lon: Double

    var coordinate: CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: lat, longitude: lon)
    }
}

struct IsochroneMeta: Decodable, Hashable {
    let origin: IsochroneOrigin?
    let depart_at: String?
    let service_date: String?
    let weekday: String?
    let params: IsochroneParams?
    let stops_loaded: Int?
    let stops_reached: Int?
    let stops_first_reached_by_transit: Int?
    let rounds: Int?
    let timetable_queries: Int?
    let truncated: Bool?
    let grid: IsochroneGrid?
    let cached: Bool?
    let elapsed_ms: Double?
    /// Present only when the server has something the map cannot show. See the
    /// header note — never suppress this.
    let note: String?

    /// True when transit contributed nothing and the shape is walking reach.
    /// Derived from the count rather than from `note`'s wording, so a reworded
    /// server string cannot silently disable the warning.
    var isWalkingOnly: Bool {
        (stops_first_reached_by_transit ?? -1) == 0
    }
}

/// A decoded isochrone: bands in paint order plus everything the server said
/// about how it got them.
struct Isochrone {
    let bands: [IsochroneBand]
    let meta: IsochroneMeta

    /// Every coordinate across every band — used to frame the camera. Bands are
    /// nested, so the widest one alone would do; taking all of them costs one
    /// pass and cannot go wrong if the server ever stops nesting them.
    var allCoordinates: [CLLocationCoordinate2D] {
        bands.flatMap { band -> [CLLocationCoordinate2D] in
            switch band.geometry {
            case .multiPolygon(let polys): return polys.flatMap { $0.flatMap { $0 } }
            case .polygon(let rings):      return rings.flatMap { $0 }
            default:                        return []
            }
        }
    }
}

// MARK: - Wire decoding

/// The raw FeatureCollection. Internal to this file — callers get `Isochrone`.
struct IsochroneResponse: Decodable {
    let features: [WireFeature]?
    let meta: IsochroneMeta?

    struct WireFeature: Decodable {
        let geometry: WireGeometry?
        let properties: WireProps?
    }

    struct WireGeometry: Decodable {
        let type: String?
        /// Nested coordinate arrays. `AnyCodable` unwraps them to `[Any]`,
        /// which is exactly what `Geometry(rawType:coords:)` consumes.
        let coordinates: AnyCodable?
    }

    struct WireProps: Decodable {
        let band_min: Int?
        let stops_within: Int?
    }

    /// Drop features whose geometry will not parse rather than failing the
    /// whole response: a malformed band should cost that band, not the answer.
    /// Order is preserved — the server emits largest-first and that is paint
    /// order.
    func decoded() -> Isochrone {
        let bands: [IsochroneBand] = (features ?? []).compactMap { f in
            guard let type = f.geometry?.type,
                  let raw = f.geometry?.coordinates?.value,
                  let geom = Geometry(rawType: type, coords: raw),
                  let minutes = f.properties?.band_min
            else { return nil }
            return IsochroneBand(bandMin: minutes,
                                 stopsWithin: f.properties?.stops_within ?? 0,
                                 geometry: geom)
        }
        return Isochrone(bands: bands,
                         meta: meta ?? IsochroneMeta(
                            origin: nil, depart_at: nil, service_date: nil,
                            weekday: nil, params: nil, stops_loaded: nil,
                            stops_reached: nil,
                            stops_first_reached_by_transit: nil, rounds: nil,
                            timetable_queries: nil, truncated: nil, grid: nil,
                            cached: nil, elapsed_ms: nil, note: nil))
    }
}
