import Foundation
import Combine

/// Wraps URLSessionWebSocketTask against the JapanOSINT backend's /ws
/// endpoint, exposing typed AsyncStreams for the panels that consume live
/// events (live vehicles, follow log, camera discovery).
@MainActor
final class WebSocketClient: ObservableObject {
    @Published private(set) var isConnected: Bool = false
    @Published private(set) var lastError: String?

    /// When true, drop all data-bearing events (live vehicles, follow, camera
    /// discoveries, earthquakes) without forwarding them to subscribers. The
    /// WS task stays connected so we get an immediate stream once the gate
    /// reopens. Driven by PlaybackState.isReplaying from MapTab.
    @Published var gateLiveEvents: Bool = false

    private var task: URLSessionWebSocketTask?
    private var session: URLSession = .shared
    private var reconnectAttempts = 0
    private var explicitlyDisconnected = false

    // Subject-based broadcast so multiple views can subscribe to the same feed.
    private let liveVehiclesSubject = PassthroughSubject<LiveVehicleEvent, Never>()
    private let followSubject       = PassthroughSubject<FollowEvent, Never>()
    private let cameraSubject       = PassthroughSubject<CameraEvent, Never>()
    private let earthquakeSubject   = PassthroughSubject<EarthquakeEvent, Never>()

    var liveVehicles: AnyPublisher<LiveVehicleEvent, Never> { liveVehiclesSubject.eraseToAnyPublisher() }
    var follow:       AnyPublisher<FollowEvent, Never>      { followSubject.eraseToAnyPublisher() }
    var cameras:      AnyPublisher<CameraEvent, Never>      { cameraSubject.eraseToAnyPublisher() }
    var earthquakes:  AnyPublisher<EarthquakeEvent, Never>  { earthquakeSubject.eraseToAnyPublisher() }

    func connect(baseURL: String) {
        explicitlyDisconnected = false
        guard let url = wsURL(from: baseURL) else {
            lastError = "Bad backend URL"
            return
        }
        task?.cancel(with: .goingAway, reason: nil)
        let t = session.webSocketTask(with: url)
        task = t
        t.resume()
        isConnected = true
        lastError = nil
        reconnectAttempts = 0
        receiveLoop()
    }

    func disconnect() {
        explicitlyDisconnected = true
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        isConnected = false
    }

    private func wsURL(from baseURL: String) -> URL? {
        let trimmed = baseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        let swapped: String
        if trimmed.hasPrefix("https://") {
            swapped = "wss://" + trimmed.dropFirst("https://".count)
        } else if trimmed.hasPrefix("http://") {
            swapped = "ws://"  + trimmed.dropFirst("http://".count)
        } else {
            swapped = "ws://" + trimmed
        }
        return URL(string: swapped + "/ws")
    }

    private func receiveLoop() {
        guard let task else { return }
        task.receive { [weak self] result in
            guard let self else { return }
            Task { @MainActor in
                switch result {
                case .failure(let err):
                    self.lastError = err.localizedDescription
                    self.isConnected = false
                    self.scheduleReconnect()
                case .success(let msg):
                    switch msg {
                    case .data(let d):    self.handle(data: d)
                    case .string(let s):  self.handle(data: Data(s.utf8))
                    @unknown default:     break
                    }
                    self.receiveLoop()
                }
            }
        }
    }

    private func scheduleReconnect() {
        guard !explicitlyDisconnected else { return }
        reconnectAttempts += 1
        let delay = min(30, pow(2.0, Double(reconnectAttempts)))
        Task { @MainActor in
            try? await Task.sleep(nanoseconds: UInt64(delay * 1_000_000_000))
            guard !self.explicitlyDisconnected else { return }
            // We don't have the baseURL stored; the app re-connects via onChange
            // when the URL flips. As a passive fallback, mark not-connected so
            // the UI surfaces it.
            self.isConnected = false
        }
    }

    /// Backend wraps every push as { "type": "...", ...payload }.
    private func handle(data: Data) {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = obj["type"] as? String else { return }
        // Always honour the lifecycle "connected" handshake regardless of
        // gate state — we need to know the socket is healthy.
        if type == "connected" { isConnected = true; return }
        // Time-slider replay gate: drop data-bearing events but keep the
        // connection warm so it picks up live data the instant we return.
        if gateLiveEvents { return }
        switch type {
        case "vehicle", "live_vehicle":
            if let ev: LiveVehicleEvent = decode(obj) { liveVehiclesSubject.send(ev) }
        case "collector_hit", "follow", "fetch":
            if let ev: FollowEvent = decode(obj) { followSubject.send(ev) }
        case "camera_discovered":
            // Server payload is { type, kind, channel, camera: <GeoJSON Feature>, ... }.
            // CameraEvent is flat, so unwrap the Feature and rename keys to match.
            if let ev = CameraEvent.fromBroadcast(envelope: obj) { cameraSubject.send(ev) }
        case "camera_run_start", "camera_channel_done", "camera_run_end":
            // Run-lifecycle envelopes — no per-camera payload, ignore for now.
            break
        case "earthquake", "jma_earthquake":
            if let ev: EarthquakeEvent = decode(obj) { earthquakeSubject.send(ev) }
        default:
            break
        }
    }

    private func decode<T: Decodable>(_ obj: [String: Any]) -> T? {
        guard let data = try? JSONSerialization.data(withJSONObject: obj) else { return nil }
        return try? JSONDecoder().decode(T.self, from: data)
    }
}

// ── Realtime payload types ─────────────────────────────────────────────────

struct LiveVehicleEvent: Decodable, Hashable {
    let id: String
    let kind: String?           // "plane" | "train" | "subway" | "bus" | "ship"
    let lat: Double
    let lon: Double
    let heading: Double?
    let speed: Double?
    let label: String?
    let timestamp: String?
    let delay_s: Int?
    let delay_kind: String?     // "arrival" | "departure" | nil
    let alert_header: String?
    let alert_text: String?
    /// Full GeoJSON-style properties blob for plane events — server-side
    /// `planeAdsbPoller` ships the same shape `unified-flights` static layer
    /// returns (icao24, callsign, airline, altitude_ft, military_tags, …) so
    /// popups have everything without an extra fetch. Nil for non-plane
    /// kinds (carriages don't carry rich metadata yet).
    let properties: [String: AnyCodable]?

    // Hashable conformance: AnyCodable values aren't Hashable, so hash on
    // identity-stable scalars only. The full properties bag is only used by
    // popups, which read it directly — never compared via Set.
    static func == (lhs: LiveVehicleEvent, rhs: LiveVehicleEvent) -> Bool {
        lhs.id == rhs.id && lhs.lat == rhs.lat && lhs.lon == rhs.lon
            && lhs.heading == rhs.heading && lhs.timestamp == rhs.timestamp
    }
    func hash(into hasher: inout Hasher) {
        hasher.combine(id); hasher.combine(lat); hasher.combine(lon)
        hasher.combine(heading); hasher.combine(timestamp)
    }
}

struct EarthquakeEvent: Decodable, Hashable {
    let id: String?
    let lat: Double?
    let lon: Double?
    let magnitude: Double?
    let depth_km: Double?
    let intensity: String?
    let timestamp: String?
    let region: String?
}
