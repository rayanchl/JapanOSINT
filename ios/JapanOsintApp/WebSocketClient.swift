import Foundation
import Combine

/// Wraps URLSessionWebSocketTask against the JapanOSINT backend's /ws
/// endpoint, exposing typed AsyncStreams for the panels that consume live
/// events (live vehicles, follow log, camera discovery).
@MainActor
final class WebSocketClient: ObservableObject {

    /// Observed lifecycle of the socket — as opposed to assumed. The old
    /// `isConnected` was set the instant `resume()` was called, before any
    /// handshake, so it reported "connected" against a route that does not
    /// exist server-side.
    enum Status: Equatable {
        /// Switched off for this build (`BuildConfig.realtimeWebSocketEnabled`).
        case disabled
        /// Never started, or explicitly disconnected.
        case idle
        /// Transport is open; the server's `{"type":"connected"}` frame has
        /// not arrived yet.
        case connecting
        /// Handshake received — the push stream is genuinely live.
        case connected
        /// Retries exhausted. **Not** an app-level offline state: REST is
        /// independent and unaffected.
        case unavailable
    }

    @Published private(set) var status: Status =
        BuildConfig.realtimeWebSocketEnabled ? .idle : .disabled
    @Published private(set) var lastError: String?

    /// True ONLY once the server's handshake frame has arrived.
    var isConnected: Bool { status == .connected }

    /// Human-readable, honest one-liner for status chrome. Deliberately never
    /// says "offline" — the absence of a push socket is not an offline app.
    var statusLabel: String {
        switch status {
        case .disabled:    return "Realtime off"
        case .idle:        return "Realtime idle"
        case .connecting:  return "Realtime connecting…"
        case .connected:   return "Realtime connected"
        case .unavailable: return "Realtime unavailable"
        }
    }

    /// Backoff ceiling, retry budget, and how long we wait for the handshake
    /// before treating a silent socket as a failed attempt.
    private static let maxBackoffSeconds: Double = 30
    private static let maxReconnectAttempts = 5
    private static let handshakeTimeout: TimeInterval = 15

    /// When true, drop all data-bearing events (live vehicles, follow, camera
    /// discoveries, earthquakes) without forwarding them to subscribers. The
    /// WS task stays connected so we get an immediate stream once the gate
    /// reopens. Driven by PlaybackState.isReplaying from MapTab.
    @Published var gateLiveEvents: Bool = false

    private var task: URLSessionWebSocketTask?
    private var session: URLSession = .shared
    private var reconnectAttempts = 0
    private var explicitlyDisconnected = false
    /// A pending reconnect is already sleeping — don't stack a second one.
    private var reconnectPending = false
    /// Bumped on every `establish`. Callbacks from a superseded socket (or a
    /// superseded handshake timer) compare against it and bail, so a stale
    /// failure can't drive the state machine of the current attempt.
    private var connectGeneration = 0
    /// Last URL we were asked to connect to, so an automatic reconnect can
    /// re-establish the socket itself rather than waiting on an external
    /// URL-change to call connect() again.
    private var lastBaseURL: String?

    // Subject-based broadcast so multiple views can subscribe to the same feed.
    private let liveVehiclesSubject = PassthroughSubject<LiveVehicleEvent, Never>()
    private let followSubject       = PassthroughSubject<FollowEvent, Never>()
    private let cameraSubject       = PassthroughSubject<CameraEvent, Never>()
    private let earthquakeSubject   = PassthroughSubject<EarthquakeEvent, Never>()

    var liveVehicles: AnyPublisher<LiveVehicleEvent, Never> { liveVehiclesSubject.eraseToAnyPublisher() }
    var follow:       AnyPublisher<FollowEvent, Never>      { followSubject.eraseToAnyPublisher() }
    var cameras:      AnyPublisher<CameraEvent, Never>      { cameraSubject.eraseToAnyPublisher() }
    var earthquakes:  AnyPublisher<EarthquakeEvent, Never>  { earthquakeSubject.eraseToAnyPublisher() }

    /// Explicit/manual connect — resets the backoff counter so a user-driven
    /// (re)connect always starts fast.
    ///
    /// No-ops while the realtime layer is gated off, which is the default: see
    /// `BuildConfig.realtimeWebSocketEnabled`. Callers do not need to check.
    func connect(baseURL: String) {
        guard BuildConfig.realtimeWebSocketEnabled else {
            explicitlyDisconnected = true
            connectGeneration &+= 1
            task?.cancel(with: .goingAway, reason: nil)
            task = nil
            status = .disabled
            return
        }
        reconnectAttempts = 0
        establish(baseURL: baseURL)
    }

    private func establish(baseURL: String) {
        explicitlyDisconnected = false
        lastBaseURL = baseURL
        guard let url = wsURL(from: baseURL) else {
            lastError = "Bad backend URL"
            status = .unavailable
            return
        }
        connectGeneration &+= 1
        let gen = connectGeneration
        task?.cancel(with: .goingAway, reason: nil)
        let t = session.webSocketTask(with: url)
        task = t
        t.resume()
        // NOT `.connected`: the transport being resumed says nothing about
        // whether anything is listening. Only the server's handshake frame
        // (see `handle(data:)`) promotes us to `.connected`.
        status = .connecting
        lastError = nil
        receiveLoop(generation: gen)
        armHandshakeTimeout(generation: gen)
    }

    /// A socket that opens but never handshakes would otherwise sit in
    /// `.connecting` forever. Treat the silence as a failed attempt so the
    /// retry budget actually runs out and we settle on `.unavailable`.
    private func armHandshakeTimeout(generation gen: Int) {
        Task { @MainActor [weak self] in
            try? await Task.sleep(
                nanoseconds: UInt64(Self.handshakeTimeout * 1_000_000_000))
            guard let self else { return }
            guard gen == self.connectGeneration, self.status == .connecting else { return }
            self.lastError = "No handshake from \(self.lastBaseURL ?? "backend")/ws"
            self.scheduleReconnect()
        }
    }

    func disconnect() {
        explicitlyDisconnected = true
        connectGeneration &+= 1
        reconnectPending = false
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        status = BuildConfig.realtimeWebSocketEnabled ? .idle : .disabled
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

    private func receiveLoop(generation gen: Int) {
        guard let task, gen == connectGeneration else { return }
        task.receive { [weak self] result in
            guard let self else { return }
            Task { @MainActor in
                // A callback from a socket we've already replaced must not
                // touch the current attempt's state.
                guard gen == self.connectGeneration else { return }
                switch result {
                case .failure(let err):
                    self.lastError = err.localizedDescription
                    if self.status == .connected { self.status = .connecting }
                    self.scheduleReconnect()
                case .success(let msg):
                    switch msg {
                    case .data(let d):    self.handle(data: d)
                    case .string(let s):  self.handle(data: Data(s.utf8))
                    @unknown default:     break
                    }
                    self.receiveLoop(generation: gen)
                }
            }
        }
    }

    /// Capped exponential backoff **with a give-up threshold**.
    ///
    /// Without the threshold this was an unbounded 30 s connect→fail loop that
    /// ran for the entire lifetime of the process against a route the server
    /// doesn't serve. After `maxReconnectAttempts` we settle on `.unavailable`
    /// and stop; `connect(baseURL:)` (app foreground, backend-URL change,
    /// re-auth) is what resets the budget.
    private func scheduleReconnect() {
        guard !explicitlyDisconnected, !reconnectPending else { return }
        guard reconnectAttempts < Self.maxReconnectAttempts else {
            giveUp()
            return
        }
        reconnectAttempts += 1
        reconnectPending = true
        // Exponential backoff capped at 30s, plus 0–1s of jitter so a fleet
        // of clients reconnecting after a server blip doesn't thundering-herd
        // the backend on synchronised boundaries.
        let backoff = min(Self.maxBackoffSeconds, pow(2.0, Double(reconnectAttempts)))
        let delay = backoff + Double.random(in: 0...1)
        let gen = connectGeneration
        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(delay * 1_000_000_000))
            guard let self else { return }
            self.reconnectPending = false
            // Superseded by a newer establish()/disconnect() while we slept.
            guard gen == self.connectGeneration else { return }
            guard !self.explicitlyDisconnected, let url = self.lastBaseURL else { return }
            // Re-establish without resetting reconnectAttempts so backoff keeps
            // growing while the endpoint stays unreachable; the counter is
            // cleared again only once we get a confirmed healthy handshake
            // (see `handle(data:)` "connected" case → reconnectAttempts = 0).
            self.establish(baseURL: url)
        }
    }

    private func giveUp() {
        reconnectPending = false
        connectGeneration &+= 1
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        status = .unavailable
    }

    /// Backend wraps every push as { "type": "...", ...payload }.
    private func handle(data: Data) {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = obj["type"] as? String else { return }
        // Always honour the lifecycle "connected" handshake regardless of
        // gate state — we need to know the socket is healthy.
        if type == "connected" {
            status = .connected
            // Confirmed healthy: clear backoff so the next blip recovers fast.
            reconnectAttempts = 0
            lastError = nil
            return
        }
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
