import SwiftUI
import MapKit
import Combine

struct MapTab: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var registry: LayerRegistry
    @EnvironmentObject var ws: WebSocketClient
    @EnvironmentObject var nav: MapNavigation
    @EnvironmentObject var stats: FeatureStats
    @EnvironmentObject var playback: PlaybackState
    @Environment(\.theme) private var theme

    @State private var cameraPosition: MapCameraPosition = .region(
        MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 36.2, longitude: 138.25),
            span: MKCoordinateSpan(latitudeDelta: 14, longitudeDelta: 14)
        )
    )
    @State private var visibleRect: MKMapRect?
    @State private var featuresByLayer: [String: [GeoFeature]] = [:]
    @State private var layerLoadState: [String: LayerLoadState] = [:]
    @State private var errorMessage: String?

    enum LayerLoadState: Equatable {
        case loading
        case cancelled  // shown as a red ✗ badge for ~10s before auto-clearing
    }
    @StateObject private var liveVehicles = LiveVehiclesStore()
    @StateObject private var searchModel = GeocodeSearchModel()

    @State private var selectedFeature: GeoFeature?
    @State private var probedCoordinate: CLLocationCoordinate2D?
    /// Pin dropped by a cross-tab "Show on map" action. Persists until
    /// replaced by another nav request — tapping it opens the feature's
    /// popup, but it does not auto-present.
    @State private var spotlightFeature: GeoFeature?

    @State private var showLayers = false
    @State private var lookAroundScene: MKLookAroundScene?
    @State private var lookAroundUnavailable = false
    @State private var mapMode: MapMode = .explore

    enum MapMode: String, CaseIterable, Identifiable {
        case explore, driving, transit, satellite
        var id: String { rawValue }
        var label: String { rawValue.capitalized }
        var systemImage: String {
            switch self {
            case .explore:   return "map"
            case .driving:   return "car.fill"
            case .transit:   return "tram.fill"
            case .satellite: return "globe.americas.fill"
            }
        }
    }

    @State private var jstNow = Date()
    private let jstTimer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    /// Drives the pulsing screen-wide replay border. Flipped on a repeating
    /// `easeInOut` animation while `playback.isReplaying`; toggled off
    /// otherwise so the border collapses to its base width.
    @State private var replayPulseOn: Bool = false

    var body: some View {
        ZStack {
            navStack
            replayBorder
        }
    }

    private var navStack: some View {
        NavigationStack {
            mapView
                // Declare the floating top bar as a safe-area inset so the
                // built-in map controls (compass, 3D toggle) reposition
                // *below* it instead of being hidden behind.
                .safeAreaInset(edge: .top, spacing: 0) {
                    topBar
                        .padding(.horizontal)
                        .padding(.top, 8)
                }
                .background(theme.surface.ignoresSafeArea())
                .task {
                    liveVehicles.bind(to: ws)
                    // Only fetch layers we don't already have data for. This
                    // makes the .task idempotent — if SwiftUI re-runs it when
                    // the tab regains focus, we don't flash "loading" badges
                    // for layers that are already populated.
                    let needsLoad = settings.activeLayerIds.filter { featuresByLayer[$0] == nil }
                    if !needsLoad.isEmpty {
                        await fetchLayers(needsLoad)
                    }
                }
                .onReceive(ws.liveVehicles) { ev in
                    // Plane updates flow into the static unified-flights
                    // layer instead of the live overlay so they share one
                    // toggle (the Layers panel) and one render path (the
                    // heading-rotated pin). Carriages (train/subway/bus)
                    // continue to flow through LiveVehiclesContent.
                    guard ev.kind?.lowercased() == "plane" else { return }
                    mergePlaneEvent(ev)
                }
                .onReceive(nav.$pendingFlyTo) { coord in
                    guard let coord else { return }
                    flyTo(coord)
                    nav.pendingFlyTo = nil
                }
                .onReceive(nav.$pendingPresent) { feat in
                    guard let feat else { return }
                    spotlightFeature = feat
                    nav.pendingPresent = nil
                }
                .safeAreaInset(edge: .bottom, spacing: 0) {
                    TimeSliderView()
                        .padding(.horizontal, 12)
                        .padding(.vertical, 6)
                }
                .onChange(of: playback.isReplaying) { _, replaying in
                    withAnimation(.spring(response: 0.3, dampingFraction: 0.78)) {
                        ws.gateLiveEvents = replaying
                    }
                    // Drive the screen-wide pulse: start the repeating
                    // line-width animation on entry, snap it off on exit.
                    if replaying {
                        withAnimation(.easeInOut(duration: 1.4).repeatForever(autoreverses: true)) {
                            replayPulseOn = true
                        }
                    } else {
                        withAnimation(.easeOut(duration: 0.3)) {
                            replayPulseOn = false
                        }
                    }
                    // Re-sync now: pull a fresh snapshot whether we just entered
                    // or just exited replay so the map shows the right slice.
                    invalidateAndRefetch()
                }
                .onChange(of: playback.at) { _, _ in
                    if playback.isReplaying { invalidateAndRefetch() }
                }
                .onChange(of: playback.window) { _, _ in
                    if playback.isReplaying { invalidateAndRefetch() }
                }
                .onChange(of: settings.activeLayerIds) { old, new in
                    // Diff-aware reload: only fetch newly-added layers, drop
                    // features for removed ones. No more sledgehammer refetch
                    // of every active layer on every toggle.
                    let removed = old.subtracting(new)
                    let added   = new.subtracting(old)
                    for id in removed {
                        featuresByLayer.removeValue(forKey: id)
                        layerLoadState.removeValue(forKey: id)
                        stats.clear(layerId: id)
                    }
                    if !added.isEmpty {
                        Task { await fetchLayers(added) }
                    }
                }
                .onChange(of: settings.liveTrainsEnabled)    { _, _ in clearIfAllOff() }
                .onChange(of: settings.liveSubwaysEnabled)   { _, _ in clearIfAllOff() }
                .onChange(of: settings.liveBusesEnabled)     { _, _ in clearIfAllOff() }
                .onReceive(jstTimer) { jstNow = $0 }
                .sheet(item: $selectedFeature) { feat in
                    NavigationStack {
                        popup(for: feat)
                    }
                    .presentationDetents([.medium, .large])
                }
                .sheet(isPresented: $showLayers) {
                    NavigationStack { LayersTab() }
                }
                .sheet(item: $lookAroundScene) { scene in
                    LookAroundPreview(initialScene: scene)
                        .ignoresSafeArea()
                }
                .alert("Street view not available here",
                       isPresented: $lookAroundUnavailable) {
                    Button("OK", role: .cancel) {}
                } message: {
                    Text("Apple Look Around has no imagery for this location.")
                }
                .navigationBarHidden(true)
        }
    }

    // MARK: - Map canvas

    private var mapView: some View {
        Map(position: $cameraPosition) {
            ForEach(visibleFeatures, id: \.id) { feat in
                content(for: feat)
            }
            if settings.liveTrainsEnabled
                || settings.liveSubwaysEnabled
                || settings.liveBusesEnabled {
                LiveVehiclesContent(store: liveVehicles, theme: theme, settings: settings)
            }
            if let coord = probedCoordinate {
                Annotation("Map center", coordinate: coord, anchor: .bottom) {
                    pin(symbol: "mappin", color: theme.accent, opacity: 1)
                }
            }
            if let spot = spotlightFeature, case .point(let c) = spot.geometry {
                // Declared after `visibleFeatures`, so MapContent ordering
                // already places this on top of the regular pins. Drawn at
                // 1.5× so the flown-to pin clearly stands out.
                Annotation(spot.displayName, coordinate: c, anchor: .bottom) {
                    pin(
                        symbol: registry.symbol(for: spot.layerId),
                        color: registry.color(for: spot.layerId),
                        opacity: 1,
                        scale: 1.5
                    )
                    .onTapGesture { selectedFeature = spot }
                }
            }
        }
        .mapStyle(currentMapStyle)
        .mapControls {
            MapPitchToggle()
            MapCompass()
        }
        .onMapCameraChange(frequency: .onEnd) { ctx in
            // Track the rect only — used by the menu's "Reverse-geocode
            // center" item. Layer fetches are no longer viewport-driven.
            visibleRect = ctx.rect
            // Drop the probed pin + address card as soon as the user pans
            // or zooms away from the resolved point. ε ≈ 1e-6° (~10 cm)
            // ignores micro-jitter while firing on any real gesture.
            if let probed = probedCoordinate {
                let center = ctx.rect.midCoordinate
                let dLat = abs(center.latitude  - probed.latitude)
                let dLon = abs(center.longitude - probed.longitude)
                if dLat > 1e-6 || dLon > 1e-6 {
                    probedCoordinate = nil
                }
            }
        }
    }

    private func clearIfAllOff() {
        if !settings.liveTrainsEnabled
            && !settings.liveSubwaysEnabled
            && !settings.liveBusesEnabled {
            liveVehicles.clear()
        }
    }

    /// Apply an incoming WS plane event to the unified-flights feature
    /// array. Match by ICAO24 (the WS event's `id`); update geometry +
    /// heading + speed in place, or append a fresh feature if this is the
    /// first time we've seen this aircraft. No-op when the layer isn't
    /// currently active — flipping the toggle on triggers a fresh REST
    /// fetch which seeds the array, and subsequent WS pushes pick up from
    /// there.
    private func mergePlaneEvent(_ ev: LiveVehicleEvent) {
        let layerId = "unified-flights"
        guard settings.activeLayerIds.contains(layerId) else { return }
        var feats = featuresByLayer[layerId] ?? []

        let updated = featureFromPlaneEvent(ev, existing: feats.first { f in
            (f.properties["icao24"]?.value as? String) == ev.id
        })
        if let idx = feats.firstIndex(where: { f in
            (f.properties["icao24"]?.value as? String) == ev.id
        }) {
            feats[idx] = updated
        } else {
            feats.append(updated)
        }
        featuresByLayer[layerId] = feats
    }

    /// Build a fresh GeoFeature from a plane WS event. Carries over any
    /// pre-existing properties from the same aircraft (so AeroDataBox
    /// schedule fields persist across position updates), then overlays
    /// the live event's `properties` blob (icao24, callsign, altitude,
    /// military_tags …) and stamps a current `heading`. Geometry comes
    /// straight from the event's lat/lon.
    private func featureFromPlaneEvent(_ ev: LiveVehicleEvent,
                                       existing: GeoFeature?) -> GeoFeature {
        var props = existing?.properties ?? [:]
        if let evProps = ev.properties {
            for (k, v) in evProps { props[k] = v }
        }
        if props["icao24"] == nil {
            props["icao24"] = AnyCodable(ev.id)
        }
        if let label = ev.label, props["callsign"] == nil {
            props["callsign"] = AnyCodable(label)
        }
        if let h = ev.heading {
            props["heading"] = AnyCodable(h)
        }
        return GeoFeature(
            id: existing?.id ?? "ADSB_LIVE_\(ev.id)",
            layerId: "unified-flights",
            geometry: .point(.init(latitude: ev.lat, longitude: ev.lon)),
            properties: props
        )
    }

    @MapContentBuilder
    private func content(for feat: GeoFeature) -> some MapContent {
        let opacity = settings.opacity(for: feat.layerId)
        let color = registry.color(for: feat.layerId)
        let symbol = registry.symbol(for: feat.layerId)

        switch feat.geometry {
        case .point(let c):
            // Aircraft on the unified-flights layer carry a `heading` property
            // (degrees, 0 = N) from the OpenSky live feed. Rotate the pin so
            // each plane points along its direction of progression. Anchor
            // .center (instead of .bottom) so the pin pivots around its
            // coordinate rather than swinging on its base.
            // SF Symbol `airplane` natively points east, so subtract 90° to
            // map the aviation 0°=N convention onto the glyph.
            if feat.layerId == "unified-flights" {
                let heading = (feat.properties["heading"]?.value as? Double)
                    ?? (feat.properties["heading"]?.value as? Int).map(Double.init)
                    ?? 0
                Annotation(feat.displayName, coordinate: c, anchor: .center) {
                    pin(symbol: symbol, color: color, opacity: opacity)
                        .rotationEffect(.degrees(heading - 90))
                        .animation(.linear(duration: 1), value: heading)
                        .onTapGesture { selectedFeature = feat }
                }
                .tag(feat.id)
            } else {
                Annotation(feat.displayName, coordinate: c, anchor: .bottom) {
                    pin(symbol: symbol, color: color, opacity: opacity)
                        .onTapGesture { selectedFeature = feat }
                }
                .tag(feat.id)
            }

        case .multiPoint(let coords):
            ForEach(Array(coords.enumerated()), id: \.offset) { idx, c in
                Annotation("\(feat.displayName) #\(idx + 1)", coordinate: c, anchor: .bottom) {
                    pin(symbol: symbol, color: color, opacity: opacity)
                        .onTapGesture { selectedFeature = feat }
                }
            }

        case .lineString(let coords):
            MapPolyline(coordinates: coords)
                .stroke(color.opacity(opacity), lineWidth: 2)

        case .multiLineString(let lines):
            ForEach(Array(lines.enumerated()), id: \.offset) { _, coords in
                MapPolyline(coordinates: coords)
                    .stroke(color.opacity(opacity), lineWidth: 2)
            }

        case .polygon(let rings):
            if let outer = rings.first {
                MapPolygon(coordinates: outer)
                    .foregroundStyle(color.opacity(opacity * 0.25))
                    .stroke(color.opacity(opacity), lineWidth: 1.2)
            }

        case .multiPolygon(let polys):
            ForEach(Array(polys.enumerated()), id: \.offset) { _, rings in
                if let outer = rings.first {
                    MapPolygon(coordinates: outer)
                        .foregroundStyle(color.opacity(opacity * 0.22))
                        .stroke(color.opacity(opacity), lineWidth: 1)
                }
            }
        }
    }

    private func pin(symbol: String, color: Color, opacity: Double, scale: CGFloat = 1) -> some View {
        mapPinView(symbol: symbol, color: color, opacity: opacity, scale: scale)
    }

    // MARK: - Top card (Apple-native)

    private var topBar: some View {
        VStack(spacing: 8) {
            // zIndex lifts the search row above its siblings so the dropdown
            // overlay (hosted on searchRow itself) floats over the status row
            // and loading pills below it instead of being drawn under them.
            searchRow
                .overlay(alignment: .topLeading) {
                    if searchModel.showResults
                        && (!searchModel.hits.isEmpty || !searchModel.featureHits.isEmpty) {
                        GeocodeSearchResultsDropdown(
                            model: searchModel,
                            onPick: { coord in flyTo(coord) },
                            onPickFeature: { feat in
                                if let coord = feat.geometry.anchor ?? feat.geometry.centroid {
                                    flyTo(coord)
                                }
                                spotlightFeature = feat
                            }
                        )
                    }
                }
                .zIndex(1)
            statusRow

            if let coord = probedCoordinate {
                reverseCard(coord)
            }
            loadingRows
            if let err = errorMessage {
                Text(err)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
                    .lineLimit(2)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 10)
        // Match the system tab bar's translucency exactly — `.bar` is Apple's
        // material specifically tuned for bar surfaces (same as TabView).
        .background(.bar,
                    in: RoundedRectangle(cornerRadius: 18, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .strokeBorder(Color.primary.opacity(0.08), lineWidth: 0.5)
        )
        .shadow(color: .black.opacity(0.18), radius: 12, y: 4)
        .animation(.easeInOut(duration: 0.2), value: layerLoadState.keys.sorted())
    }

    private static let loadingRowsVisibleLimit = 5

    @ViewBuilder
    private var loadingRows: some View {
        if !layerLoadState.isEmpty {
            let allIds = layerLoadState.keys.sorted()
            let visibleIds = Array(allIds.prefix(Self.loadingRowsVisibleLimit))
            let overflow = allIds.count - visibleIds.count
            VStack(spacing: 4) {
                ForEach(visibleIds, id: \.self) { id in
                    let state = layerLoadState[id] ?? .loading
                    HStack(spacing: 8) {
                        Image(systemName: registry.symbol(for: id))
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(registry.color(for: id))
                            .frame(width: 18)
                        Text(LayerRegistry.displayName(forId: id))
                            .font(.caption)
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                        Spacer()
                        switch state {
                        case .loading:
                            ProgressView().controlSize(.mini)
                        case .cancelled:
                            Image(systemName: "xmark")
                                .font(.caption2.bold())
                                .foregroundStyle(.white)
                                .frame(width: 16, height: 16)
                                .background(theme.danger, in: Circle())
                                .accessibilityLabel("Cancelled")
                        }
                    }
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(theme.surfaceElevated, in: Capsule())
                    .transition(.asymmetric(
                        insertion: .opacity.combined(with: .scale(scale: 0.9, anchor: .top)),
                        removal: .opacity
                    ))
                }
                if overflow > 0 {
                    Text("+\(overflow) more collector\(overflow == 1 ? "" : "s") running")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 2)
                        .transition(.opacity)
                }
            }
        }
    }

    /// Search field + inline trailing actions, all on a single row.
    private var searchRow: some View {
        HStack(spacing: 8) {
            GeocodeSearchBar(
                model: searchModel,
                onSubmit: { runGeocodeSearch() },
                onChange: { text in
                    searchModel.debounce(
                        text,
                        api: { API(baseURL: settings.backendBaseURL) },
                        featureSearch: { searchActiveFeatures(matching: $0) }
                    )
                }
            )
            layersButton
            reverseGeocodeButton
            moreMenu
        }
    }

    private func runGeocodeSearch() {
        Task {
            await searchModel.runSearch(
                api: API(baseURL: settings.backendBaseURL),
                featureSearch: { searchActiveFeatures(matching: $0) }
            )
        }
    }

    /// Local case-insensitive name match across loaded features for layers
    /// the user has currently enabled. Prefix matches outrank substring
    /// matches; the dropdown is hard-capped to keep results scannable.
    private func searchActiveFeatures(matching query: String, limit: Int = 8) -> [GeoFeature] {
        let q = query.trimmingCharacters(in: .whitespaces).lowercased()
        guard !q.isEmpty else { return [] }
        var prefixHits: [GeoFeature] = []
        var containsHits: [GeoFeature] = []
        outer: for layerId in settings.activeLayerIds {
            guard let feats = featuresByLayer[layerId] else { continue }
            for f in feats {
                let name = f.displayName.lowercased()
                if name.hasPrefix(q) {
                    prefixHits.append(f)
                } else if name.contains(q) {
                    containsHits.append(f)
                }
                if prefixHits.count >= limit { break outer }
            }
        }
        return Array((prefixHits + containsHits).prefix(limit))
    }

    private var reverseGeocodeButton: some View {
        Button {
            if let center = visibleRect?.midCoordinate {
                probedCoordinate = center
            }
        } label: {
            Image(systemName: "mappin.and.ellipse")
                .font(.body.weight(.semibold))
                .foregroundStyle(.tint)
                .frame(width: 36, height: 36)
                .background(.thinMaterial, in: Circle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Reverse-geocode center")
    }

    private var currentMapStyle: MapStyle {
        switch mapMode {
        case .explore:
            return .standard(elevation: .realistic, pointsOfInterest: .excludingAll)
        case .driving:
            return .standard(elevation: .realistic,
                             pointsOfInterest: .excludingAll,
                             showsTraffic: true)
        case .transit:
            return .standard(elevation: .realistic,
                             pointsOfInterest: .including([.publicTransport]))
        case .satellite:
            return .hybrid(elevation: .realistic, pointsOfInterest: .excludingAll)
        }
    }

    private func openLookAround() {
        let center = visibleRect?.midCoordinate
            ?? CLLocationCoordinate2D(latitude: 35.68, longitude: 139.76)
        Task { @MainActor in
            do {
                let req = MKLookAroundSceneRequest(coordinate: center)
                if let scene = try await req.scene {
                    lookAroundScene = scene
                } else {
                    lookAroundUnavailable = true
                }
            } catch {
                lookAroundUnavailable = true
            }
        }
    }

    private var layersButton: some View {
        Button { showLayers = true } label: {
            HStack(spacing: 4) {
                Image(systemName: "square.3.stack.3d")
                Text("\(settings.activeLayerIds.count)")
                    .font(.subheadline.weight(.semibold))
                    .monospacedDigit()
            }
            .foregroundStyle(.tint)
            .frame(height: 36)
            .padding(.horizontal, 12)
            .background(.thinMaterial, in: Capsule())
        }
        .buttonStyle(.plain)
    }

    private var moreMenu: some View {
        Menu {
            Picker("Map style", selection: $mapMode) {
                ForEach(MapMode.allCases) { m in
                    Label(m.label, systemImage: m.systemImage).tag(m)
                }
            }
            Divider()
            Button(action: openLookAround) {
                Label("Street view", systemImage: "binoculars.fill")
            }
        } label: {
            Image(systemName: "ellipsis")
                .font(.body.weight(.semibold))
                .foregroundStyle(.tint)
                .frame(width: 36, height: 36)
                .background(.thinMaterial, in: Circle())
        }
    }

    /// Compact status footer: WS dot + label, JST time, feature count.
    /// Uses Apple's secondary text styling so it reads as supporting info.
    private var statusRow: some View {
        HStack(spacing: 10) {
            HStack(spacing: 4) {
                Circle()
                    .fill(ws.isConnected ? theme.success : theme.danger)
                    .frame(width: 6, height: 6)
                Text(ws.isConnected ? "LIVE" : "OFFLINE")
                    .font(.caption2.weight(.semibold))
                    .tracking(0.6)
                    .foregroundStyle(ws.isConnected ? theme.success : theme.danger)
            }

            Text("·").font(.caption2).foregroundStyle(.tertiary)

            Text(jstFormatted)
                .font(.caption2.monospacedDigit())
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .contentTransition(.numericText())
                .animation(.snappy, value: jstNow)

            Spacer()

            Text("\(visibleFeatures.count) features")
                .font(.caption2)
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
        .padding(.horizontal, 4)
    }

    private func reverseCard(_ coord: CLLocationCoordinate2D) -> some View {
        HStack(alignment: .top, spacing: 6) {
            CoordinateAddressView(coordinate: coord)
            Button { probedCoordinate = nil } label: {
                Image(systemName: "xmark.circle.fill")
                    .foregroundStyle(.secondary)
            }
            .buttonStyle(.plain)
        }
        .padding(8)
        .background(.thinMaterial, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }

    private var jstFormatted: String {
        let f = DateFormatter()
        f.dateFormat = "HH:mm:ss"
        f.timeZone = TimeZone(identifier: "Asia/Tokyo")
        return "\(f.string(from: jstNow)) JST"
    }

    // MARK: - Popup dispatch

    @ViewBuilder
    private func popup(for feat: GeoFeature) -> some View {
        featurePopup(for: feat)
    }

    // MARK: - Data flow

    private var visibleFeatures: [GeoFeature] {
        let pointCap = max(1, settings.maxFeaturesPerLayer)
        let shapeCap = max(1, settings.maxLinesPolygonsPerLayer)
        let paddedRect: MKMapRect? = visibleRect.map {
            $0.insetBy(dx: -$0.size.width * 0.25, dy: -$0.size.height * 0.25)
        }

        // Even stride-decimation to a target count.
        func decimate(_ feats: [GeoFeature], to cap: Int) -> [GeoFeature] {
            guard feats.count > cap else { return feats }
            let stride = max(1, feats.count / cap)
            return feats.enumerated()
                .compactMap { $0.offset.isMultiple(of: stride) ? $0.element : nil }
        }

        // Centroid-in-rect cull (cheap, works for any geometry kind).
        func cull(_ feats: [GeoFeature]) -> [GeoFeature] {
            guard let rect = paddedRect else { return feats }
            return feats.filter { f in
                guard let c = f.geometry.centroid else { return false }
                return rect.contains(MKMapPoint(c))
            }
        }

        var out: [GeoFeature] = []
        for id in settings.activeLayerIds {
            guard let feats = featuresByLayer[id] else { continue }

            // Split by geometry family so each gets its own cap.
            var points: [GeoFeature] = []
            var shapes: [GeoFeature] = []
            points.reserveCapacity(feats.count)
            shapes.reserveCapacity(feats.count)
            for f in feats {
                switch f.geometry {
                case .point, .multiPoint: points.append(f)
                default:                  shapes.append(f)
                }
            }

            out.append(contentsOf: decimate(cull(points), to: pointCap))
            out.append(contentsOf: decimate(cull(shapes), to: shapeCap))
        }
        return out
    }

    /// Fetch a set of layers concurrently. Used both at cold-start (every
    /// active layer) and from the diff-aware `onChange` (only the added set).
    private func fetchLayers(_ ids: any Sequence<String>) async {
        await withTaskGroup(of: Void.self) { group in
            for id in ids {
                // Fallback when the registry doesn't have the layer (e.g.
                // server-stripped UX merges like unified-subways that
                // `AppSettings.hiddenFollowers` activates programmatically).
                // The collector is still reachable via /api/data/:id —
                // LayerDef.dataEndpoint only needs the id.
                let layer = registry.layer(for: id)
                    ?? LayerDef(id: id, name: id, category: nil, sources: nil, temporal: nil, liveOnly: nil)
                group.addTask { @MainActor in
                    await self.fetchOne(layer)
                }
            }
        }
    }

    /// Drop time-coded + liveOnly layers' caches and refetch them under the
    /// current PlaybackState. Static layers are left untouched — they look
    /// the same at every slider position so there's no point refetching.
    private func invalidateAndRefetch() {
        let ids = settings.activeLayerIds
        var toRefetch: Set<String> = []
        for id in ids {
            let layer = registry.layer(for: id)
            // No metadata → assume time-coded (default server disposition).
            // Static layers explicitly skip; liveOnly layers refetch so they
            // can return their empty-FC reply during replay.
            if layer?.isStatic == true { continue }
            featuresByLayer.removeValue(forKey: id)
            toRefetch.insert(id)
        }
        guard !toRefetch.isEmpty else { return }
        Task { await fetchLayers(toRefetch) }
    }

    private func fetchOne(_ layer: LayerDef) async {
        let id = layer.id
        layerLoadState[id] = .loading
        // Cached server responses can return in <50ms — the loading-state
        // flicker would collapse into one render cycle, hiding siblings that
        // haven't finished. Hold each row visible for ~350ms minimum.
        let startedAt = Date()
        do {
            let api = API(baseURL: settings.backendBaseURL)
            // Pass the current slider position into the layer fetch. The
            // server honours `at`+`window` for time-coded layers, ignores them
            // for static, and short-circuits to an empty FC for liveOnly.
            let fc = try await api.data(
                for: layer,
                at: playback.at,
                windowSeconds: playback.isReplaying ? playback.window.seconds : nil
            )
            featuresByLayer[id] = fc.features
            stats.record(
                layerId: id,
                total: fc.features.count,
                bySource: extractBySource(fc.meta?["bySource"]?.value)
            )
            errorMessage = nil
            await holdLoadingFloor(startedAt: startedAt)
            layerLoadState.removeValue(forKey: id)
        } catch is CancellationError {
            await holdLoadingFloor(startedAt: startedAt)
            markCancelled(id)
        } catch let url as URLError where url.code == .cancelled {
            await holdLoadingFloor(startedAt: startedAt)
            markCancelled(id)
        } catch {
            errorMessage = "[\(registry.displayName(for: layer))] \(error.localizedDescription)"
            await holdLoadingFloor(startedAt: startedAt)
            layerLoadState.removeValue(forKey: id)
        }
    }

    /// Coerces the dynamic `_meta.bySource` payload (server emits it for
    /// unified-* collectors) into a `[String: Int]` ready for `FeatureStats`.
    /// Numeric fields can come through as Int or Double depending on JSON
    /// serialization; fall through with an empty dict otherwise.
    private func extractBySource(_ value: Any?) -> [String: Int]? {
        guard let dict = value as? [String: Any] else { return nil }
        var out: [String: Int] = [:]
        for (k, v) in dict {
            if let n = v as? Int       { out[k] = n }
            else if let d = v as? Double { out[k] = Int(d) }
            else if let n = v as? NSNumber { out[k] = n.intValue }
        }
        return out.isEmpty ? nil : out
    }

    private func holdLoadingFloor(startedAt: Date) async {
        let elapsed = Date().timeIntervalSince(startedAt)
        if elapsed < 0.35 {
            try? await Task.sleep(for: .milliseconds(Int((0.35 - elapsed) * 1000)))
        }
    }

    /// Show the red ✗ badge for ~10s, then clear — unless a newer fetch for
    /// the same layer takes over in the meantime (it'll set `.loading` and
    /// our staleness check below leaves it alone).
    private func markCancelled(_ id: String) {
        layerLoadState[id] = .cancelled
        Task { @MainActor in
            try? await Task.sleep(for: .seconds(10))
            if layerLoadState[id] == .cancelled {
                layerLoadState[id] = nil
            }
        }
    }

    private func flyTo(_ coord: CLLocationCoordinate2D) {
        withAnimation(.easeInOut(duration: 0.6)) {
            cameraPosition = .region(
                MKCoordinateRegion(
                    center: coord,
                    span: MKCoordinateSpan(latitudeDelta: 0.08, longitudeDelta: 0.08)
                )
            )
        }
    }

    /// Screen-wide pulsing border, drawn only while the time slider is in
    /// replay mode. Sits in a sibling layer above the NavigationStack with
    /// `.ignoresSafeArea()` so it traces the device's full perimeter; the
    /// rounded-rect corner radius matches the iPhone screen mask so the OS
    /// clip is invisible. `allowsHitTesting(false)` keeps map gestures
    /// unobstructed.
    @ViewBuilder
    private var replayBorder: some View {
        if playback.isReplaying {
            // Deliberately oversized corner radius: the OS clips the view
            // to the device's actual display mask, so an oversized rounded
            // rect produces a curve that "flows" further into the corners
            // rather than meeting the screen edge with a sharp transition.
            RoundedRectangle(cornerRadius: 55, style: .continuous)
                .strokeBorder(theme.accent.opacity(0.85), lineWidth: replayPulseOn ? 4 : 1.5)
                .ignoresSafeArea()
                .allowsHitTesting(false)
                .transition(.opacity)
        }
    }
}

extension MKMapRect {
    var midCoordinate: CLLocationCoordinate2D {
        MKMapPoint(x: midX, y: midY).coordinate
    }
}

// Required so SwiftUI's `.sheet(item:)` can drive presentation off
// the optional fetched scene.
extension MKLookAroundScene: Identifiable {
    public var id: ObjectIdentifier { ObjectIdentifier(self) }
}

/// Shared marker style used on the main map and on the Saved tab's map.
/// Layer color tinted circle with the layer's SF Symbol stamped on top.
/// `scale` is applied to the circle and the glyph; default 1 (22pt circle).
@ViewBuilder
func mapPinView(symbol: String, color: Color, opacity: Double = 1, scale: CGFloat = 1) -> some View {
    let size: CGFloat = 22 * scale
    ZStack {
        Circle()
            .fill(color.opacity(opacity))
            .frame(width: size, height: size)
        Image(systemName: symbol)
            .font(.system(size: 11 * scale, weight: .bold))
            .foregroundStyle(.white)
    }
    .overlay(
        Circle().stroke(.white.opacity(0.85), lineWidth: 1)
    )
}
