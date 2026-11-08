import SwiftUI

/// Pulls departures for a station from `/api/transit/station/<key>/summary`
/// (core/transitapi.c `station_summary`) and auto-refreshes. It used to call
/// `/api/transit/departures/<key>` — a route the server never had — through a
/// bare URLSession with no bearer, so every board read "No live board" whatever
/// the station. It goes through `API.request` now: the same auth, the same
/// credential guard, the same timeout handling as every other call.
struct DeparturesBoard: View {
    let stationKey: String
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var rows: [Departure] = []
    @State private var loading = false
    @State private var note: String?

    struct Departure: Identifiable, Hashable {
        let id: String
        let time: String
        let route: String
        let destination: String
        let delayMin: Int?
        let track: String?
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            PopupSectionHeader("Departures", icon: "tram.fill") {
                if loading { ProgressView().scaleEffect(0.7) }
                Button {
                    Task { await load() }
                } label: { Image(systemName: "arrow.clockwise") }
                .accessibilityLabel("Reload departures")
            }

            if rows.isEmpty {
                Text(note ?? "No live departures.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
            } else {
                ForEach(rows) { r in
                    HStack(alignment: .firstTextBaseline) {
                        Text(r.time)
                            .font(.system(.callout, design: .monospaced).bold())
                            .foregroundStyle(theme.accent)
                            .frame(width: 56, alignment: .leading)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(r.route).font(.subheadline.bold())
                            Text("→ \(r.destination)").font(.caption).foregroundStyle(theme.textMuted)
                        }
                        Spacer()
                        if let d = r.delayMin, d != 0 {
                            Text("+\(d)m")
                                .font(.caption2.bold())
                                .padding(.horizontal, 6).padding(.vertical, 2)
                                .background(d > 0 ? theme.danger : theme.success,
                                            in: Capsule())
                                .foregroundStyle(.white)
                        }
                        if let t = r.track {
                            Text(t).font(.caption2).foregroundStyle(theme.textMuted)
                        }
                    }
                    .padding(.vertical, 6)
                    .padding(.horizontal, 8)
                    .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
                }
            }
        }
        .task { await load() }
        .task {
            while !Task.isCancelled {
                let seconds = max(1, settings.departuresRefreshSeconds)
                try? await Task.sleep(for: .seconds(seconds))
                await load()
            }
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        let api = apiClient.api
        do {
            let url = try api.makeURL("/api/transit/station/\(api.esc(stationKey))/summary")
            let data = try await api.request(url, timeout: 10)
            let parsed = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
            // GTFS rows: {trip_id, departure_sec, wall_sec, headsign, route_short,
            // route_long, delay_s}; ODPT rows are a second list on the same body.
            let gtfs = (parsed?["departures"] as? [[String: Any]]) ?? []
            let odpt = (parsed?["odpt_departures"] as? [[String: Any]]) ?? []
            let arr = gtfs.isEmpty ? odpt : gtfs
            rows = arr.prefix(max(1, settings.departuresShown)).enumerated().map { idx, r in
                let wall = (r["wall_sec"] as? NSNumber)?.intValue
                    ?? (r["departure_sec"] as? NSNumber)?.intValue
                let time = wall.map { String(format: "%02d:%02d", ($0 / 3600) % 24, ($0 / 60) % 60) }
                    ?? (r["time"] as? String) ?? (r["departure_time"] as? String) ?? "—"
                let delayS = (r["delay_s"] as? NSNumber)?.intValue
                return Departure(
                    id: (r["trip_id"] as? String) ?? (r["id"] as? String) ?? "row-\(idx)",
                    time: time,
                    route: (r["route_short"] as? String) ?? (r["route_long"] as? String)
                        ?? (r["route"] as? String) ?? (r["line"] as? String) ?? "",
                    destination: (r["headsign"] as? String) ?? (r["destination"] as? String) ?? "",
                    delayMin: delayS.map { $0 / 60 } ?? (r["delay_min"] as? Int),
                    track: (r["platform"] as? String) ?? (r["track"] as? String)
                )
            }
            note = rows.isEmpty ? "No upcoming departures." : nil
        } catch APIError.http(let code, _) where code == 404 {
            note = "No live board for this station."
            rows = []
        } catch {
            note = "Could not fetch departures: \(error.localizedDescription)"
        }
    }
}
