import SwiftUI
import MapKit
import Combine

/// Aggregates live-vehicle WebSocket pushes into an animated annotation set.
/// Mounted as a child MapContent inside MapTab via @StateObject + computed
/// MapContentBuilder. Kept separate so the heavy update path is isolated.
@MainActor
final class LiveVehiclesStore: ObservableObject {
    @Published private(set) var vehicles: [String: LiveVehicleEvent] = [:]
    private var cancellable: AnyCancellable?

    func bind(to ws: WebSocketClient) {
        cancellable?.cancel()
        cancellable = ws.liveVehicles.sink { [weak self] ev in
            Task { @MainActor [weak self] in
                self?.vehicles[ev.id] = ev
            }
        }
    }

    func clear() { vehicles.removeAll() }
}

struct LiveVehiclesContent: MapContent {
    let store: LiveVehiclesStore
    let theme: ThemePalette
    let settings: AppSettings

    var body: some MapContent {
        ForEach(Array(store.vehicles.values).filter { isEnabled($0.kind) }, id: \.id) { v in
            Annotation(v.label ?? v.id,
                       coordinate: CLLocationCoordinate2D(latitude: v.lat, longitude: v.lon),
                       anchor: .center) {
                ZStack(alignment: .topTrailing) {
                    ZStack {
                        Circle()
                            .fill(color(for: v.kind))
                            .frame(width: 14, height: 14)
                        Image(systemName: symbol(for: v.kind))
                            .font(.system(size: 8, weight: .bold))
                            .foregroundStyle(.white)
                    }
                    .rotationEffect(.degrees(v.heading ?? 0))
                    if let d = v.delay_s, d > 0 {
                        Text("+\(d / 60)m")
                            .font(.system(size: 9, weight: .bold))
                            .padding(.horizontal, 4).padding(.vertical, 1)
                            .background(d > 300 ? theme.danger : theme.warning, in: Capsule())
                            .foregroundStyle(.white)
                            .offset(x: 6, y: -6)
                    }
                }
                .animation(.linear(duration: 1), value: v.lat)
                .animation(.linear(duration: 1), value: v.lon)
            }
        }
    }

    private func isEnabled(_ kind: String?) -> Bool {
        // Planes (and ships, which currently have no live source) render via
        // the static `unified-flights` layer with WS-merged position updates.
        // The live overlay is now strictly for "carriages" — trains/subways/
        // buses, the moving dots that travel along their static route lines.
        // `liveCarriagesEnabled` is a master gate; the per-mode toggles let
        // users hide just the kinds they don't care about while keeping the
        // overall feature on.
        guard settings.liveCarriagesEnabled else { return false }
        switch (kind ?? "").lowercased() {
        case "train":  return settings.liveTrainsEnabled
        case "subway": return settings.liveSubwaysEnabled
        case "bus":    return settings.liveBusesEnabled
        default:       return false
        }
    }

    private func color(for kind: String?) -> Color {
        switch (kind ?? "").lowercased() {
        case "plane":  return theme.accentAlt
        case "ship":   return theme.accent
        case "train":  return theme.warning
        case "subway": return theme.accent
        case "bus":    return theme.accentAlt
        default:       return theme.accent
        }
    }
    private func symbol(for kind: String?) -> String {
        switch (kind ?? "").lowercased() {
        case "plane":  return "airplane"
        case "ship":   return "ferry"
        case "train":  return "tram.fill"
        case "subway": return "tram.tunnel.fill"
        case "bus":    return "bus.fill"
        default:       return "circle.fill"
        }
    }
}
