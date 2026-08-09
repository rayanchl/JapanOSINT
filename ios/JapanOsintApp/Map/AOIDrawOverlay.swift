import SwiftUI
import MapKit
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// Area-of-interest drawing (roadmap 9).
//
// Presented over the map (sheet / full-screen cover) from `MapTab`. It owns its
// own `Map` so it can install a tap gesture without touching MapTab's gesture
// stack, and it starts at whatever region the caller was already looking at, so
// "draw a fence around here" means here.
//
// Two shapes, matching exactly what the backend's `alert_eval_shape_check()`
// accepts:
//   • polygon — tap to drop vertices, ≥3, then close the ring. Sent as
//               [[lon,lat], …]; the server auto-closes and rejects a
//               zero-area (collinear) ring.
//   • circle  — tap the centre, then drag (or use the slider) for the radius.
//               Sent as {"lat":…,"lon":…,"radius_m":…}.
// Note the lon/lat ORDER on polygon points: GeoJSON order, which is what the C
// validator reads. Swapping it would silently produce a fence in the sea.
//
// Two exits, deliberately separate:
//   • "Save as area" → POST /api/aoi, so the shape can be reused by name.
//   • "Alert me here" → hands the geometry back through `onAlertHere` and
//     dismisses. This file does NOT know about `AlertEditor`; the caller opens
//     a prefilled editor with what it receives. That keeps the map free of a
//     dependency on the alerts UI.
// ─────────────────────────────────────────────────────────────────────────────

/// A finished shape, in every form a caller might need: the raw geometry, the
/// `[w,s,e,n]` bbox that `AlertPredicate.bbox` expects, and the JSON payload
/// `API.aoiCreate(name:kind:geometry:)` wants.
struct DrawnAOI {
    enum Kind: String {
        case polygon, circle
    }

    let kind: Kind
    /// Polygon vertices in draw order (open ring — the server closes it).
    /// Empty for a circle.
    let ring: [CLLocationCoordinate2D]
    /// Circle centre. `nil` for a polygon.
    let center: CLLocationCoordinate2D?
    /// Circle radius in metres. `nil` for a polygon.
    let radiusMeters: Double?
    /// West, south, east, north — the order `AlertPredicate.bbox` uses.
    let bbox: [Double]

    /// The `geometry` value for `API.aoiCreate`. Polygon → `[[lon,lat], …]`,
    /// circle → `{"lat":…,"lon":…,"radius_m":…}`.
    var geometryPayload: Any {
        switch kind {
        case .polygon:
            return ring.map { [$0.longitude, $0.latitude] }
        case .circle:
            guard let c = center, let r = radiusMeters else { return [:] }
            return ["lat": c.latitude, "lon": c.longitude, "radius_m": r]
        }
    }

    /// Short human description, e.g. "Polygon · 6 points" / "Circle · 2.4 km".
    var summary: String {
        switch kind {
        case .polygon:
            return "Polygon · \(ring.count) point\(ring.count == 1 ? "" : "s")"
        case .circle:
            return "Circle · \(DrawnAOI.formatRadius(radiusMeters ?? 0))"
        }
    }

    static func formatRadius(_ metres: Double) -> String {
        if metres < 1000 { return "\(Int(metres.rounded())) m" }
        return String(format: "%.1f km", metres / 1000)
    }

    static func polygon(_ points: [CLLocationCoordinate2D]) -> DrawnAOI? {
        guard points.count >= 3 else { return nil }
        let lons = points.map(\.longitude)
        let lats = points.map(\.latitude)
        guard let w = lons.min(), let e = lons.max(),
              let s = lats.min(), let n = lats.max() else { return nil }
        return DrawnAOI(kind: .polygon, ring: points, center: nil,
                        radiusMeters: nil, bbox: [w, s, e, n])
    }

    static func circle(center: CLLocationCoordinate2D,
                       radiusMeters: Double) -> DrawnAOI? {
        guard radiusMeters > 0 else { return nil }
        // MKCoordinateRegion does the metres→degrees conversion for us,
        // including the latitude-dependent longitude widening.
        let region = MKCoordinateRegion(center: center,
                                        latitudinalMeters: radiusMeters * 2,
                                        longitudinalMeters: radiusMeters * 2)
        let w = max(-180, center.longitude - region.span.longitudeDelta / 2)
        let e = min(180,  center.longitude + region.span.longitudeDelta / 2)
        let s = max(-90,  center.latitude  - region.span.latitudeDelta / 2)
        let n = min(90,   center.latitude  + region.span.latitudeDelta / 2)
        return DrawnAOI(kind: .circle, ring: [], center: center,
                        radiusMeters: radiusMeters, bbox: [w, s, e, n])
    }
}

struct AOIDrawOverlay: View {

    /// Called with the finished shape when the user picks "Alert me here". The
    /// caller is expected to open a prefilled `AlertEditor`; this view dismisses
    /// itself immediately afterwards.
    let onAlertHere: (DrawnAOI) -> Void
    /// Called after a successful POST /api/aoi, so a list elsewhere can insert
    /// the new area without a round trip.
    var onSaved: ((AreaOfInterest) -> Void)? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    enum DrawMode: String, CaseIterable, Identifiable {
        case polygon, circle
        var id: String { rawValue }
        var label: String { self == .polygon ? "Polygon" : "Circle" }
        var systemImage: String {
            self == .polygon ? "pentagon" : "circle.dashed"
        }
    }

    @State private var camera: MapCameraPosition
    @State private var mode: DrawMode = .polygon

    // Polygon state
    @State private var points: [CLLocationCoordinate2D] = []
    @State private var ringClosed = false

    // Circle state
    @State private var circleCenter: CLLocationCoordinate2D?
    @State private var circleRadius: Double = 1000       // metres
    @State private var draggingRadius = false

    /// Result of the last save attempt — success and failure share one alert.
    struct SaveOutcome {
        let ok: Bool
        let message: String
    }

    // Persistence state
    @State private var showNamePrompt = false
    @State private var areaName = ""
    @State private var saving = false
    @State private var outcome: SaveOutcome?

    private static let minRadius: Double = 50
    private static let maxRadius: Double = 200_000

    init(initialRegion: MKCoordinateRegion? = nil,
         onAlertHere: @escaping (DrawnAOI) -> Void,
         onSaved: ((AreaOfInterest) -> Void)? = nil) {
        self.onAlertHere = onAlertHere
        self.onSaved = onSaved
        let region = initialRegion ?? MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 35.68, longitude: 139.76),
            span: MKCoordinateSpan(latitudeDelta: 0.6, longitudeDelta: 0.6)
        )
        _camera = State(initialValue: .region(region))
    }

    var body: some View {
        NavigationStack {
            mapCanvas
                .safeAreaInset(edge: .top, spacing: 0) {
                    instructionBar
                        .padding(.horizontal, Space.md)
                        .padding(.top, Space.sm)
                }
                .safeAreaInset(edge: .bottom, spacing: 0) {
                    controlBar
                        .padding(.horizontal, Space.md)
                        .padding(.bottom, Space.sm)
                }
                .navigationTitle("Draw area")
                .compatInlineTitle()
                .toolbar {
                    ToolbarItem(placement: .compatLeading) {
                        Button("Cancel") { dismiss() }
                    }
                    ToolbarItem(placement: .compatPrimary) {
                        Button("Clear") { clearAll() }
                            .disabled(!hasAnything)
                    }
                }
        }
        .compatSheetSizing(.large)
        .alert("Name this area", isPresented: $showNamePrompt) {
            TextField("e.g. Kanto corridor", text: $areaName)
            // Deliberately not `.disabled` — alert buttons don't reliably
            // honour it. `saveArea()` refuses an empty name and says so.
            Button("Save") { Task { await saveArea() } }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Saved areas can be reused by any alert rule, and stay editable from Areas of interest.")
        }
        // One outcome alert rather than two stacked ones — chaining several
        // `.alert` modifiers onto the same view is unreliable, and success and
        // failure are mutually exclusive answers to the same action anyway.
        .alert(outcome?.ok == true ? "Area saved" : "Couldn't save area",
               isPresented: Binding(get: { outcome != nil },
                                    set: { if !$0 { outcome = nil } }),
               presenting: outcome) { result in
            if result.ok {
                Button("Done") { outcome = nil; dismiss() }
                Button("Keep drawing", role: .cancel) { outcome = nil }
            } else {
                Button("OK", role: .cancel) { outcome = nil }
            }
        } message: { result in
            Text(result.message)
        }
    }

    // MARK: - Map

    private var mapCanvas: some View {
        MapReader { proxy in
            Map(position: $camera) {
                shapeContent
            }
            .mapStyle(.standard(elevation: .flat, pointsOfInterest: .excludingAll))
            .onTapGesture(coordinateSpace: .local) { point in
                guard let coord = proxy.convert(point, from: .local) else { return }
                handleTap(coord)
            }
            // Only present while the user is dragging the radius. It swallows
            // map panning on purpose — that phase is modal, and "Done" ends it.
            .overlay {
                if mode == .circle, draggingRadius, let centre = circleCenter {
                    Color.clear
                        .contentShape(Rectangle())
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { value in
                                    guard let p = proxy.convert(value.location,
                                                                from: .local) else { return }
                                    circleRadius = clampRadius(metres(from: centre, to: p))
                                }
                        )
                }
            }
        }
    }

    @MapContentBuilder
    private var shapeContent: some MapContent {
        if mode == .polygon {
            if points.count >= 2 {
                MapPolyline(coordinates: ringClosed && points.count >= 3
                            ? points + [points[0]]
                            : points)
                    .stroke(theme.accent, lineWidth: 2)
            }
            if ringClosed, points.count >= 3 {
                MapPolygon(coordinates: points)
                    .foregroundStyle(theme.accent.opacity(0.22))
                    .stroke(theme.accent, lineWidth: 2)
            }
            ForEach(Array(points.enumerated()), id: \.offset) { idx, coord in
                MapKit.Annotation("Vertex \(idx + 1)", coordinate: coord, anchor: .center) {
                    vertexHandle(index: idx)
                }
            }
        } else {
            if let centre = circleCenter {
                MapCircle(center: centre, radius: max(circleRadius, 1))
                    .foregroundStyle(theme.accent.opacity(0.22))
                    .stroke(theme.accent, lineWidth: 2)
                MapKit.Annotation("Centre", coordinate: centre, anchor: .center) {
                    centreHandle
                }
            }
        }
    }

    private func vertexHandle(index: Int) -> some View {
        ZStack {
            Circle()
                .fill(theme.surface)
                .frame(width: 18, height: 18)
            Circle()
                .strokeBorder(theme.accent, lineWidth: 2)
                .frame(width: 18, height: 18)
            Text("\(index + 1)")
                .font(.system(size: 9, weight: .bold, design: .monospaced))
                .foregroundStyle(theme.accent)
        }
        .accessibilityLabel("Vertex \(index + 1)")
    }

    private var centreHandle: some View {
        ZStack {
            Circle()
                .fill(theme.accent)
                .frame(width: 12, height: 12)
            Circle()
                .strokeBorder(.white.opacity(0.9), lineWidth: 1.5)
                .frame(width: 12, height: 12)
        }
        .accessibilityLabel("Circle centre")
    }

    // MARK: - Instruction bar

    private var instructionBar: some View {
        HStack(spacing: Space.sm) {
            Image(systemName: mode.systemImage)
                .foregroundStyle(theme.accent)
            Text(instruction)
                .font(.caption)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
            if let drawn = finishedShape {
                Pill(text: drawn.summary.uppercased(), tone: .success,
                     icon: "checkmark", maxWidth: 190)
            }
        }
        .padding(Space.sm)
        .mapBarSurface()
    }

    private var instruction: String {
        switch mode {
        case .polygon:
            if ringClosed { return "Ring closed. Save it, or alert on it." }
            if points.isEmpty { return "Tap the map to drop the first vertex." }
            if points.count < 3 { return "Tap to add vertices — 3 minimum." }
            return "Tap to add more, or close the ring."
        case .circle:
            if circleCenter == nil { return "Tap the map to place the centre." }
            if draggingRadius { return "Drag anywhere to size the circle, then tap Done." }
            return "Radius \(DrawnAOI.formatRadius(circleRadius)). Adjust below, or alert on it."
        }
    }

    // MARK: - Controls

    private var controlBar: some View {
        VStack(spacing: Space.sm) {
            Picker("Shape", selection: $mode) {
                ForEach(DrawMode.allCases) { m in
                    Text(m.label).tag(m)
                }
            }
            .pickerStyle(.segmented)
            .onChange(of: mode) { _, _ in clearAll() }

            if mode == .polygon {
                polygonControls
            } else {
                circleControls
            }

            if let drawn = finishedShape {
                finishRow(drawn)
            }
        }
        .padding(Space.sm)
        .mapBarSurface()
    }

    private var polygonControls: some View {
        HStack(spacing: Space.sm) {
            Button {
                undoPoint()
            } label: {
                Label("Undo point", systemImage: "arrow.uturn.backward")
                    .font(.caption.weight(.semibold))
            }
            .buttonStyle(.bordered)
            .disabled(points.isEmpty)

            Button {
                ringClosed = true
                Haptics.success()
            } label: {
                Label("Close ring", systemImage: "checkmark.circle")
                    .font(.caption.weight(.semibold))
            }
            .buttonStyle(.bordered)
            .disabled(points.count < 3 || ringClosed)

            Spacer(minLength: 0)

            Text("\(points.count) pt")
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
        }
    }

    private var circleControls: some View {
        VStack(spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                Button {
                    draggingRadius.toggle()
                    Haptics.selection()
                } label: {
                    Label(draggingRadius ? "Done" : "Drag radius",
                          systemImage: draggingRadius ? "checkmark" : "hand.draw")
                        .font(.caption.weight(.semibold))
                }
                .buttonStyle(.bordered)
                .disabled(circleCenter == nil)

                Spacer(minLength: 0)

                Text(DrawnAOI.formatRadius(circleRadius))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.text)
            }
            // Label-less init on purpose: the readout above already names the
            // value, and the label-taking overload's trailing-closure position
            // is easy to get wrong.
            Slider(value: $circleRadius, in: Self.minRadius...Self.maxRadius)
                .disabled(circleCenter == nil)
                .tint(theme.accent)
                .accessibilityLabel("Circle radius")
        }
    }

    private func finishRow(_ drawn: DrawnAOI) -> some View {
        HStack(spacing: Space.sm) {
            Button {
                areaName = ""
                showNamePrompt = true
            } label: {
                Label("Save as area", systemImage: "square.and.arrow.down")
                    .font(.caption.weight(.semibold))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .disabled(saving)

            Button {
                Haptics.success()
                onAlertHere(drawn)
                dismiss()
            } label: {
                Label("Alert me here", systemImage: "bell.badge")
                    .font(.caption.weight(.semibold))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(saving)
        }
    }

    // MARK: - Drawing actions

    private func handleTap(_ coord: CLLocationCoordinate2D) {
        switch mode {
        case .polygon:
            // A closed ring is final — tapping again would silently reopen a
            // shape the user just finished.
            guard !ringClosed else { return }
            points.append(coord)
            Haptics.selection()
        case .circle:
            circleCenter = coord
            draggingRadius = true
            Haptics.selection()
        }
    }

    private func undoPoint() {
        if ringClosed {
            ringClosed = false
        } else if !points.isEmpty {
            points.removeLast()
        }
        Haptics.selection()
    }

    private func clearAll() {
        points = []
        ringClosed = false
        circleCenter = nil
        circleRadius = 1000
        draggingRadius = false
    }

    private var hasAnything: Bool {
        !points.isEmpty || circleCenter != nil
    }

    private var finishedShape: DrawnAOI? {
        switch mode {
        case .polygon:
            guard ringClosed else { return nil }
            return DrawnAOI.polygon(points)
        case .circle:
            guard let c = circleCenter, !draggingRadius else { return nil }
            return DrawnAOI.circle(center: c, radiusMeters: circleRadius)
        }
    }

    // MARK: - Save

    private func saveArea() async {
        guard let drawn = finishedShape else {
            outcome = SaveOutcome(ok: false, message: "Finish the shape first — a polygon needs a closed ring, a circle needs a radius.")
            Haptics.error()
            return
        }
        let name = areaName.trimmingCharacters(in: .whitespaces)
        guard !name.isEmpty else {
            outcome = SaveOutcome(ok: false, message: "An area needs a name.")
            Haptics.error()
            return
        }
        saving = true
        defer { saving = false }
        do {
            let saved = try await apiClient.api.aoiCreate(
                name: name, kind: drawn.kind.rawValue,
                geometry: drawn.geometryPayload)
            onSaved?(saved)
            outcome = SaveOutcome(
                ok: true,
                message: "“\(saved.name)” saved as a \(saved.kind) area. Any alert rule can now geofence on it.")
            Haptics.success()
        } catch let e {
            outcome = SaveOutcome(ok: false, message: e.localizedDescription)
            Haptics.error()
        }
    }

    // MARK: - Geometry helpers

    private func metres(from a: CLLocationCoordinate2D,
                        to b: CLLocationCoordinate2D) -> Double {
        CLLocation(latitude: a.latitude, longitude: a.longitude)
            .distance(from: CLLocation(latitude: b.latitude, longitude: b.longitude))
    }

    private func clampRadius(_ r: Double) -> Double {
        guard r.isFinite else { return Self.minRadius }
        return min(max(r, Self.minRadius), Self.maxRadius)
    }
}
