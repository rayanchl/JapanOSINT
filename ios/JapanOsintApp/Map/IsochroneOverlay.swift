import SwiftUI
import MapKit
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// Travel-time reachability (roadmap 32) — the client for `GET /api/isochrone`.
//
// Presented over the map from `MapTab`, and shaped like `AOIDrawOverlay`
// beside it: its own `Map`, so it can own a tap gesture without touching
// MapTab's gesture stack, starting at whatever region the caller was looking
// at. Tap to place an origin, set the budget, compute.
//
// THE HONESTY SURFACE IS THE POINT OF THIS SCREEN, not decoration on it. A
// travel-time polygon is the most confidently-wrong-looking artefact in the
// product: it is a smooth filled shape that reads as authoritative regardless
// of what produced it. Four server signals are therefore rendered where the
// user cannot miss them, and none of them is behind a disclosure:
//
//   • meta.note              — verbatim, in a banner. The server writes it when
//                              the shape is walking-only or the origin has no
//                              stops at all.
//   • meta.truncated         — the search hit an internal bound, so the shape
//                              UNDERSTATES reach. Shown as a warning pill.
//   • meta.cached            — the answer can be up to the cache TTL old.
//   • meta.params            — what the server ACTUALLY used after clamping,
//                              never what this screen asked for.
//
// A zero-band answer renders as an explicit "nothing is reachable" state with
// the origin pin still on the map — never as an empty map, which is
// indistinguishable from "you haven't pressed the button yet".
//
// Bands arrive largest-first and are painted in that order, so the widest sits
// underneath. `Isochrone.bands` is not sorted here — see IsochroneModels.swift.
// ─────────────────────────────────────────────────────────────────────────────

struct IsochroneOverlay: View {

    // NOTE — the obvious next affordance here is "show me the intel inside
    // this shape", handing the tightest band's `[w,s,e,n]` to the items list.
    // It is deliberately ABSENT rather than stubbed: `intelapi.c` has no
    // `near=`/`bbox=` filter (roadmap 32's other half was never built), so the
    // button would have nothing to call. Adding the server filter first is the
    // right order; a button wired to nothing is how dead UI gets shipped.

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    // MARK: Input state

    @State private var camera: MapCameraPosition
    @State private var origin: CLLocationCoordinate2D?

    /// Time budget in minutes. Server clamps 5…90; the slider matches that
    /// range so the UI cannot ask for something that will be silently changed.
    @State private var maxMin: Double = 30
    /// Access/transfer walking cap in metres. Server clamps 100…2000.
    @State private var maxWalkM: Double = 800
    /// Walking speed km/h. Server clamps 2.0…7.0.
    @State private var walkKmh: Double = 4.8
    /// Ride legs beyond the first. Server clamps 0…4.
    @State private var maxTransfers: Int = 2
    /// nil = depart now (server-side JST now). Set = an explicit wall-clock.
    @State private var departAt: Date?
    @State private var showDeparturePicker = false
    @State private var pickerDate = Date()

    // MARK: Result state

    @State private var result: Isochrone?
    @State private var running = false
    @State private var errorText: String?
    /// Set when the server throttled us (12/min). Counts down so the button can
    /// say when it is worth trying again instead of just failing.
    @State private var retryAfter: Int?

    private static let bandCount = 3

    init(initialRegion: MKCoordinateRegion? = nil) {
        let region = initialRegion ?? MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 35.68, longitude: 139.76),
            span: MKCoordinateSpan(latitudeDelta: 0.4, longitudeDelta: 0.4)
        )
        _camera = State(initialValue: .region(region))
    }

    var body: some View {
        NavigationStack {
            mapCanvas
                .safeAreaInset(edge: .top, spacing: 0) {
                    VStack(spacing: Space.xs) {
                        instructionBar
                        if let r = result { metaBar(r) }
                    }
                    .padding(.horizontal, Space.md)
                    .padding(.top, Space.sm)
                }
                .safeAreaInset(edge: .bottom, spacing: 0) {
                    controlBar
                        .padding(.horizontal, Space.md)
                        .padding(.bottom, Space.sm)
                }
                .navigationTitle("Travel time")
                .compatInlineTitle()
                .toolbar {
                    ToolbarItem(placement: .compatLeading) {
                        Button("Close") { dismiss() }
                    }
                    ToolbarItem(placement: .compatPrimary) {
                        Button("Clear") { clearAll() }
                            .disabled(origin == nil && result == nil)
                    }
                }
        }
        .compatSheetSizing(.large)
        .sheet(isPresented: $showDeparturePicker) { departureSheet }
    }

    // MARK: - Map

    private var mapCanvas: some View {
        MapReader { proxy in
            Map(position: $camera) {
                bandContent
                if let o = origin {
                    MapKit.Annotation("Origin", coordinate: o, anchor: .center) {
                        originHandle
                    }
                }
            }
            .mapStyle(.standard(elevation: .flat, pointsOfInterest: .excludingAll))
            .onTapGesture(coordinateSpace: .local) { point in
                guard let coord = proxy.convert(point, from: .local) else { return }
                origin = coord
                // A result belongs to the origin it was computed for. Keeping
                // it on screen after the pin moves would attribute one place's
                // reachability to another.
                result = nil
                errorText = nil
                Haptics.selection()
            }
        }
    }

    @MapContentBuilder
    private var bandContent: some MapContent {
        // Server order is paint order: widest first, underneath.
        ForEach(result?.bands ?? []) { band in
            ForEach(Array(rings(of: band.geometry).enumerated()), id: \.offset) { _, ring in
                MapPolygon(coordinates: ring)
                    .foregroundStyle(theme.accent.opacity(fillOpacity(for: band)))
                    .stroke(theme.accent.opacity(strokeOpacity(for: band)),
                            lineWidth: 1.5)
            }
        }
    }

    /// Outer rings only. Holes are dropped deliberately: MapKit's `MapPolygon`
    /// has no hole support, and drawing a hole's ring as another filled polygon
    /// would paint the unreachable pocket as reachable — the exact inversion of
    /// what the contour means. An unfilled pocket under-claims instead, which
    /// is the safe direction to be wrong.
    private func rings(of geometry: Geometry) -> [[CLLocationCoordinate2D]] {
        switch geometry {
        case .multiPolygon(let polys):
            return polys.compactMap { $0.first }.filter { $0.count >= 3 }
        case .polygon(let polyRings):
            return polyRings.first.map { [$0] }?.filter { $0.count >= 3 } ?? []
        default:
            return []
        }
    }

    /// 0 for the widest band, 1 for the tightest. Ordered against the band
    /// LIST rather than against absolute minutes, so a custom band set still
    /// grades cleanly instead of assuming 10/20/30.
    private func grade(_ band: IsochroneBand) -> Double {
        let bands = result?.bands ?? []
        guard bands.count > 1,
              let idx = bands.firstIndex(where: { $0.bandMin == band.bandMin })
        else { return 1 }
        return Double(idx) / Double(bands.count - 1)   // server order: widest first
    }

    /// One hue, graded by alpha — tighter bands read denser. A single hue is
    /// deliberate: bands are NESTED, so they stack, and two different hues
    /// stacking would produce a third colour that means nothing.
    private func fillOpacity(for band: IsochroneBand) -> Double {
        0.12 + 0.16 * grade(band)
    }

    private func strokeOpacity(for band: IsochroneBand) -> Double {
        0.55 + 0.40 * grade(band)
    }

    private var originHandle: some View {
        ZStack {
            Circle().fill(theme.accent).frame(width: 14, height: 14)
            Circle().strokeBorder(.white.opacity(0.9), lineWidth: 2)
                .frame(width: 14, height: 14)
        }
        .accessibilityLabel("Travel-time origin")
    }

    // MARK: - Instruction bar

    private var instructionBar: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: "figure.walk.circle")
                .foregroundStyle(theme.accent)
            Text(instruction)
                .font(.caption)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
            if running { ProgressView().controlSize(.small) }
        }
        .padding(Space.sm)
        .mapBarSurface()
    }

    private var instruction: String {
        if running { return "Walking the timetable — a cold origin takes a few seconds." }
        if let e = errorText { return e }
        if origin == nil { return "Tap the map to place an origin." }
        guard let r = result else {
            return "Set a budget, then compute reachability from the pin."
        }
        if r.bands.isEmpty { return "Nothing is reachable from here on this service day." }
        return "\(r.bands.count) band\(r.bands.count == 1 ? "" : "s") · \(r.meta.stops_reached ?? 0) stops reached."
    }

    // MARK: - Meta bar (the honesty surface)

    @ViewBuilder
    private func metaBar(_ r: Isochrone) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            // The server's own words, verbatim and unconditional. This is the
            // one thing on the screen that must never be summarised away.
            if let note = r.meta.note, !note.isEmpty {
                HStack(alignment: .top, spacing: Space.xs) {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .font(.caption2)
                        .foregroundStyle(theme.warning)
                    Text(note)
                        .font(.caption2)
                        .foregroundStyle(theme.text)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: Space.xs) {
                    if r.meta.truncated == true {
                        Pill(text: "TRUNCATED — UNDERSTATES REACH",
                             tone: .warning, icon: "scissors")
                    }
                    if r.meta.isWalkingOnly {
                        Pill(text: "WALKING ONLY", tone: .warning, icon: "figure.walk")
                    }
                    if r.meta.cached == true {
                        Pill(text: "CACHED", tone: .info, icon: "clock.arrow.circlepath")
                    }
                    if let wd = r.meta.weekday, let sd = r.meta.service_date {
                        Pill(text: "\(wd.uppercased()) \(sd)", tone: .neutral,
                             icon: "calendar")
                    }
                    if let dep = r.meta.depart_at {
                        Pill(text: dep, tone: .neutral, icon: "clock", maxWidth: 190)
                    }
                    if let q = r.meta.timetable_queries {
                        Pill(text: "\(q) TIMETABLE QUERIES", tone: .neutral)
                    }
                    if let ms = r.meta.elapsed_ms {
                        Pill(text: "\(Int(ms)) MS", tone: .neutral, icon: "bolt")
                    }
                    if let cell = r.meta.grid?.cell_m {
                        Pill(text: "\(Int(cell)) M GRID", tone: .neutral, icon: "grid")
                    }
                }
            }

            // What the server actually ran with, not what was requested.
            if let p = r.meta.params {
                Text(paramsSummary(p))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }

            if !r.bands.isEmpty { legend(r) }
        }
        .padding(Space.sm)
        .mapBarSurface()
    }

    private func paramsSummary(_ p: IsochroneParams) -> String {
        var parts: [String] = []
        if let v = p.max_min       { parts.append("\(v) min budget") }
        if let v = p.max_walk_m    { parts.append("\(Int(v)) m walk cap") }
        if let v = p.walk_kmh      { parts.append(String(format: "%.1f km/h", v)) }
        if let v = p.max_transfers { parts.append("\(v) transfer\(v == 1 ? "" : "s")") }
        return "server used: " + (parts.isEmpty ? "defaults" : parts.joined(separator: " · "))
    }

    private func legend(_ r: Isochrone) -> some View {
        HStack(spacing: Space.sm) {
            ForEach(r.bands) { band in
                HStack(spacing: 4) {
                    RoundedRectangle(cornerRadius: 2)
                        .fill(theme.accent.opacity(0.25 + 0.45 * grade(band)))
                        .frame(width: 12, height: 12)
                    Text(band.label)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            Spacer(minLength: 0)
        }
    }

    // MARK: - Controls

    private var controlBar: some View {
        VStack(spacing: Space.sm) {
            sliderRow(title: "Budget",
                      value: Int(maxMin).description + " min",
                      binding: $maxMin, range: 5...90, step: 5)
            sliderRow(title: "Walk cap",
                      value: Int(maxWalkM).description + " m",
                      binding: $maxWalkM, range: 100...2000, step: 100)

            HStack(spacing: Space.sm) {
                Stepper("Transfers \(maxTransfers)",
                        value: $maxTransfers, in: 0...4)
                    .font(.caption.weight(.semibold))
                    .fixedSize()
                Spacer(minLength: 0)
                Button {
                    pickerDate = departAt ?? Date()
                    showDeparturePicker = true
                } label: {
                    Label(departureLabel, systemImage: "clock")
                        .font(.caption.weight(.semibold))
                }
                .buttonStyle(.bordered)
            }

            Button {
                Task { await compute() }
            } label: {
                Label(computeLabel, systemImage: "figure.walk.circle")
                    .font(.caption.weight(.semibold))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(origin == nil || running || retryAfter != nil)
        }
        .padding(Space.sm)
        .mapBarSurface()
    }

    private var computeLabel: String {
        if running { return "Computing…" }
        if let s = retryAfter { return "Rate limited · \(s)s" }
        return result == nil ? "Compute reachability" : "Recompute"
    }

    /// JST, and labelled as such. The server reads a bare timestamp as JST
    /// (isochrone.h), so rendering the picked instant in the DEVICE's timezone
    /// would show "09:00" next to a request for 09:00 JST — the same moment
    /// only when the analyst happens to be in Japan.
    private var departureLabel: String {
        guard let d = departAt else { return "Depart now" }
        return Self.jstLabel.string(from: d) + " JST"
    }

    private static let jstZone = TimeZone(identifier: "Asia/Tokyo") ?? .current

    private static let jstLabel: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "d MMM HH:mm"
        f.timeZone = jstZone
        return f
    }()

    /// What actually goes on the wire: `YYYY-MM-DDTHH:MM` in JST.
    private static let jstWire: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "yyyy-MM-dd'T'HH:mm"
        f.timeZone = jstZone
        return f
    }()

    private func sliderRow(title: String, value: String,
                           binding: Binding<Double>,
                           range: ClosedRange<Double>, step: Double) -> some View {
        VStack(spacing: 2) {
            HStack {
                Text(title)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(theme.text)
                Spacer(minLength: 0)
                Text(value)
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            Slider(value: binding, in: range, step: step)
                .tint(theme.accent)
                .accessibilityLabel(title)
        }
    }

    private var departureSheet: some View {
        NavigationStack {
            VStack(spacing: Space.md) {
                DatePicker("Departure",
                           selection: $pickerDate,
                           displayedComponents: [.date, .hourAndMinute])
                    .datePickerStyle(.graphical)
                    .labelsHidden()
                    // Edit in JST so the wheel shows the same wall-clock the
                    // server will read. Without this the picker is device-local
                    // and silently means a different moment abroad.
                    .environment(\.timeZone, Self.jstZone)
                Text("Set in JST — the timezone every GTFS feed in this corpus publishes in. A departure in the small hours under-reports late-night service, because one service day is queried.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 0)
            }
            .padding(Space.md)
            .navigationTitle("Departure")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Depart now") {
                        departAt = nil
                        result = nil
                        showDeparturePicker = false
                    }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button("Set") {
                        departAt = pickerDate
                        result = nil
                        showDeparturePicker = false
                    }
                }
            }
        }
        .compatSheetSizing(.medium)
    }

    // MARK: - Actions

    private func clearAll() {
        origin = nil
        result = nil
        errorText = nil
        retryAfter = nil
    }

    private func compute() async {
        guard let o = origin else { return }
        running = true
        errorText = nil
        defer { running = false }

        // Even bands over the budget, matching the server's own default rule
        // — sent explicitly so the legend can be drawn before the reply lands
        // if that is ever wanted, and so a changed budget always regrades.
        let step = max(1, Int(maxMin) / Self.bandCount)
        let bands = (1...Self.bandCount).map { min(Int(maxMin), step * $0) }

        // Bare `YYYY-MM-DDTHH:MM` in JST — the same wall-clock the picker
        // showed, which is the same wall-clock isochrone.h says the server
        // reads. Nil means "now", server-side.
        let iso8601 = departAt.map { Self.jstWire.string(from: $0) }

        do {
            let iso = try await apiClient.api.isochrone(
                lat: o.latitude, lon: o.longitude,
                maxMin: Int(maxMin),
                bands: Array(Set(bands)).sorted(),
                departAt: iso8601,
                maxWalkM: Int(maxWalkM),
                walkKmh: walkKmh,
                maxTransfers: maxTransfers)
            result = iso
            fitCamera(to: iso)
            Haptics.success()
        } catch let e {
            Haptics.error()
            if let secs = API.isochroneRetryAfter(e) {
                // Drop the spinner BEFORE awaiting the countdown — the `defer`
                // above would not fire until the countdown finished, and the
                // button prefers "Computing…" over the retry text, so the UI
                // would claim to be working for a full minute of doing nothing.
                running = false
                await countDownRetry(from: secs)
            } else {
                errorText = e.localizedDescription
            }
        }
    }

    /// Twelve requests a minute is generous for a person dragging a pin and
    /// exactly the wrong thing to hide: the button says how long is left
    /// rather than failing silently and inviting another tap.
    private func countDownRetry(from seconds: Int) async {
        retryAfter = max(1, seconds)
        while let s = retryAfter, s > 0 {
            try? await Task.sleep(nanoseconds: 1_000_000_000)
            retryAfter = s - 1 > 0 ? s - 1 : nil
        }
    }

    /// Fit the camera to the widest band so the answer is visible without a
    /// pinch. No-op for an empty result — the origin pin stays where it is,
    /// which is what makes "nothing is reachable" legible as an answer rather
    /// than as a blank screen.
    private func fitCamera(to iso: Isochrone) {
        let coords = iso.allCoordinates
        guard coords.count >= 2,
              let w = coords.map(\.longitude).min(),
              let e = coords.map(\.longitude).max(),
              let s = coords.map(\.latitude).min(),
              let n = coords.map(\.latitude).max() else { return }
        let centre = CLLocationCoordinate2D(latitude: (s + n) / 2,
                                            longitude: (w + e) / 2)
        let span = MKCoordinateSpan(latitudeDelta: max(0.01, (n - s) * 1.3),
                                    longitudeDelta: max(0.01, (e - w) * 1.3))
        withAnimation(.easeInOut(duration: 0.5)) {
            camera = .region(MKCoordinateRegion(center: centre, span: span))
        }
    }
}
