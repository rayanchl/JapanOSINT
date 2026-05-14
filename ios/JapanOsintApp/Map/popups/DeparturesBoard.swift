import SwiftUI

/// Pulls departures for a station from /api/transit/departures/<key> and
/// auto-refreshes every 30s. Tolerates a missing endpoint (shows a friendly
/// note instead of an error).
struct DeparturesBoard: View {
    let stationKey: String
    @EnvironmentObject var settings: AppSettings
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
        let trimmed = settings.backendBaseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        guard let url = URL(string: "\(trimmed)/api/transit/departures/\(stationKey)") else {
            note = "Bad URL"
            return
        }
        do {
            var req = URLRequest(url: url)
            req.timeoutInterval = 10
            let (data, resp) = try await URLSession.shared.data(for: req)
            guard let http = resp as? HTTPURLResponse else { return }
            guard (200..<300).contains(http.statusCode) else {
                note = "No live board for this station."
                rows = []
                return
            }
            // Tolerant decoding — backend payload shape may vary.
            let parsed = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
            let arr = (parsed?["departures"] as? [[String: Any]]) ?? (parsed?["entries"] as? [[String: Any]]) ?? []
            rows = arr.prefix(max(1, settings.departuresShown)).enumerated().map { idx, r in
                Departure(
                    id: (r["id"] as? String) ?? "row-\(idx)",
                    time:        (r["time"] as? String) ?? (r["scheduled"] as? String) ?? "—",
                    route:       (r["route"] as? String) ?? (r["line"] as? String) ?? "",
                    destination: (r["destination"] as? String) ?? (r["headsign"] as? String) ?? "",
                    delayMin:    (r["delay_min"] as? Int) ?? (r["delay"] as? Int),
                    track:       (r["track"] as? String) ?? (r["platform"] as? String)
                )
            }
            note = rows.isEmpty ? "No upcoming trains." : nil
        } catch {
            note = "Could not fetch departures."
        }
    }
}
