import SwiftUI
import MapKit
import Combine

struct CameraDiscoveryView: View {
    let onShowOnMap: (CLLocationCoordinate2D, GeoFeature) -> Void

    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var ws: WebSocketClient
    @EnvironmentObject var saved: SavedStore
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    @State private var events: [CameraEvent] = []
    @State private var liveIds: Set<String> = []
    @State private var feedCursor: String? = nil
    @State private var loadingMore = false
    @State private var triggering = false
    @State private var triggerError: String?
    @State private var subscription: AnyCancellable?
    @State private var seeded = false
    @State private var selectedFeature: GeoFeature?
    @State private var searchText = ""

    @State private var mode: Mode = .list
    @State private var columns: Int = 2
    @State private var feedAvailability: FeedAvailability = .all
    @State private var selectedChannels: Set<String> = []
    @State private var showFilters: Bool = false
    @State private var cameraPosition: MapCameraPosition = CameraDiscoveryView.japanRegion

    private static let japanRegion: MapCameraPosition = .region(
        MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 36.2, longitude: 138.25),
            span: MKCoordinateSpan(latitudeDelta: 14, longitudeDelta: 14)
        )
    )

    enum Mode: String, CaseIterable, Identifiable {
        case list, grid, map
        var id: String { rawValue }
        var label: String { rawValue.capitalized }
    }

    enum FeedAvailability: String, CaseIterable, Identifiable {
        case all, has, none
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:  return "All"
            case .has:  return "Has feed"
            case .none: return "No feed"
            }
        }
    }

    var body: some View {
        Group {
            // WS down + no events → unified offline state. WS up + no events
            // is just a pre-discovery state (instructive empty), keep custom.
            if events.isEmpty && !ws.isConnected {
                OfflineStateView(retry: { Task { await trigger() } })
            } else {
                contentView
            }
        }
        .background(theme.surface.ignoresSafeArea())
        // Map mode reclaims the large-title space so the map gets the full
        // viewport — the title would otherwise overlap pins on first appear.
        // List/Grid keep the standard collapsing large title.
        .navigationTitle(mode == .map ? "" : "Cameras")
        .navigationBarTitleDisplayMode(mode == .map ? .inline : .automatic)
        .searchable(
            text: $searchText,
            placement: .navigationBarDrawer(displayMode: .always),
            prompt: "Search cameras"
        )
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    showFilters = true
                } label: {
                    Image(systemName: filtersAreActive
                          ? "line.3.horizontal.decrease.circle.fill"
                          : "line.3.horizontal.decrease.circle")
                }
                .accessibilityLabel("Filters")
            }
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    Task { await trigger() }
                } label: {
                    if triggering {
                        ProgressView().controlSize(.mini)
                    } else {
                        Image(systemName: "play.circle.fill")
                    }
                }
                .disabled(triggering)
                .accessibilityLabel(triggering ? "Running discovery…" : "Re-run discovery")
            }
            if !events.isEmpty {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Clear") { events.removeAll() }
                }
            }
        }
        .task {
            subscribe()
            await seed()
        }
        .onDisappear { subscription?.cancel() }
        .sheet(item: $selectedFeature) { feat in
            NavigationStack { featurePopup(for: feat, showsMiniMap: true) }
                .presentationDetents([.medium, .large])
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
    }

    // MARK: - Content dispatch

    @ViewBuilder
    private var contentView: some View {
        switch mode {
        case .list: listView
        case .grid: gridView
        case .map:  mapWithPicker
        }
    }

    private var modePicker: some View {
        Picker("Mode", selection: $mode) {
            ForEach(Mode.allCases) { m in
                Text(m.label).tag(m)
            }
        }
        .pickerStyle(.segmented)
        .padding(.horizontal)
        .padding(.top, 8)
        .padding(.bottom, 8)
    }

    private var columnsPicker: some View {
        HStack {
            Text("Columns")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
            Spacer()
            Picker("Columns", selection: $columns) {
                Text("2").tag(2)
                Text("3").tag(3)
            }
            .pickerStyle(.segmented)
            .frame(maxWidth: 160)
        }
        .padding(.horizontal)
        .padding(.bottom, 8)
    }

    private var gridColumns: [GridItem] {
        Array(repeating: GridItem(.flexible(), spacing: 10), count: columns)
    }

    // MARK: - List

    private var listView: some View {
        ScrollView {
            LazyVStack(spacing: 8) {
                modePicker
                if events.isEmpty {
                    emptyState
                } else if filteredEvents.isEmpty {
                    noMatchView
                }
                ForEach(filteredEvents.reversed()) { ev in
                    listCard(ev)
                }
                loadMoreButton
            }
            .padding(.horizontal)
            .padding(.bottom)
        }
    }

    // MARK: - Grid

    private var gridView: some View {
        ScrollView {
            VStack(spacing: 0) {
                modePicker
                columnsPicker
                if events.isEmpty {
                    emptyState.padding(.top, 40)
                } else if filteredEvents.isEmpty {
                    noMatchView.padding(.top, 40)
                } else {
                    LazyVGrid(columns: gridColumns, spacing: 10) {
                        ForEach(filteredEvents.reversed()) { ev in
                            gridCard(ev)
                        }
                    }
                    .padding(.horizontal)
                    .padding(.bottom, 10)
                    loadMoreButton
                }
            }
        }
    }

    @ViewBuilder
    private var loadMoreButton: some View {
        if feedCursor != nil {
            Button {
                Task { await loadMore() }
            } label: {
                HStack(spacing: 6) {
                    if loadingMore { ProgressView().scaleEffect(0.7) }
                    Text(loadingMore ? "Loading…" : "Load older")
                        .font(.caption)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .disabled(loadingMore)
            .padding(.top, 4)
        }
    }

    // MARK: - Map

    private var mapWithPicker: some View {
        VStack(spacing: 0) {
            modePicker
            mapView
        }
    }

    @ViewBuilder
    private var mapView: some View {
        Map(position: $cameraPosition) {
            ForEach(filteredEvents) { ev in
                if let lat = ev.lat, let lon = ev.lon {
                    Annotation(
                        ev.title ?? ev.id,
                        coordinate: CLLocationCoordinate2D(latitude: lat, longitude: lon),
                        anchor: .bottom
                    ) {
                        Button {
                            selectedFeature = feature(from: ev)
                        } label: {
                            mapPinView(
                                symbol: registry.symbol(for: "cameras"),
                                color: registry.color(for: "cameras")
                            )
                        }
                    }
                }
            }
        }
        .mapStyle(.standard(elevation: .realistic))
        .mapControls {
            MapPitchToggle()
            MapCompass()
        }
        .onAppear { fitToFiltered() }
        .onChange(of: filteredEvents.count) { _, _ in fitToFiltered() }
        .overlay(alignment: .center) {
            if filteredEvents.isEmpty {
                Text(events.isEmpty
                     ? "No cameras yet — tap the play icon to run discovery."
                     : "No cameras match the current filters.")
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .multilineTextAlignment(.center)
                    .padding(10)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                    .padding(.horizontal, 32)
            }
        }
    }

    /// Fit camera to the currently visible (filtered) cameras. Falls back to
    /// the Japan-wide region whenever the filter result is empty so the
    /// previous zoom doesn't get stuck on a stale subset.
    private func fitToFiltered() {
        let coords: [(Double, Double)] = filteredEvents.compactMap { ev in
            guard let lat = ev.lat, let lon = ev.lon else { return nil }
            return (lat, lon)
        }
        guard !coords.isEmpty else {
            cameraPosition = CameraDiscoveryView.japanRegion
            return
        }
        let lats = coords.map(\.0)
        let lons = coords.map(\.1)
        guard let minLat = lats.min(), let maxLat = lats.max(),
              let minLon = lons.min(), let maxLon = lons.max() else { return }
        let center = CLLocationCoordinate2D(
            latitude: (minLat + maxLat) / 2,
            longitude: (minLon + maxLon) / 2
        )
        let span = MKCoordinateSpan(
            latitudeDelta: max(0.05, (maxLat - minLat) * 1.4),
            longitudeDelta: max(0.05, (maxLon - minLon) * 1.4)
        )
        cameraPosition = .region(MKCoordinateRegion(center: center, span: span))
    }

    // MARK: - Cards

    private func listCard(_ ev: CameraEvent) -> some View {
        let isLive = liveIds.contains(ev.id)
        return VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 6) {
                if isLive {
                    Text("NEW")
                        .font(.caption2.bold())
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(theme.success, in: Capsule())
                        .foregroundStyle(.white)
                } else if let kind = ev.kind, kind != "historical" {
                    Text(kind.uppercased())
                        .font(.caption2.bold())
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(badgeColor(kind), in: Capsule())
                        .foregroundStyle(.white)
                }
                Text(ev.title ?? ev.id)
                    .font(.subheadline.bold())
                    .lineLimit(1)
                Spacer()
                if let ts = ev.timestamp {
                    Text(ts).font(.caption2).foregroundStyle(theme.textMuted)
                }
            }

            CameraFeedView(
                directSnapshotURLString: ev.snapshot_url,
                pageURLString: ev.url,
                youtubeID: ev.propString("youtube_id"),
                hlsURLString: ev.propString("hls_url"),
                discoveryChannel: ev.firstDiscoveryChannel,
                cameraUID: ev.id,
                originalPageURLString: ev.propString("original_page_url"),
                style: .compact,
                showsHeader: false
            )

            if let url = ev.url {
                Text(url)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
            }

            HStack(spacing: 16) {
                if let lat = ev.lat, let lon = ev.lon {
                    Text(String(format: "%.4f, %.4f", lat, lon))
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(theme.textMuted)
                    Spacer()
                    iconButton(
                        systemName: saved.contains(id: savedId(ev)) ? "star.fill" : "star",
                        tint: saved.contains(id: savedId(ev)) ? theme.warning : theme.textMuted,
                        accessibility: saved.contains(id: savedId(ev)) ? "Remove from saved" : "Save"
                    ) {
                        toggleSaved(ev)
                    }
                    iconButton(
                        systemName: "mappin.and.ellipse",
                        tint: theme.textMuted,
                        accessibility: "Show on map"
                    ) {
                        onShowOnMap(
                            CLLocationCoordinate2D(latitude: lat, longitude: lon),
                            feature(from: ev)
                        )
                    }
                } else {
                    Spacer()
                    iconButton(
                        systemName: saved.contains(id: savedId(ev)) ? "star.fill" : "star",
                        tint: saved.contains(id: savedId(ev)) ? theme.warning : theme.textMuted,
                        accessibility: saved.contains(id: savedId(ev)) ? "Remove from saved" : "Save"
                    ) {
                        toggleSaved(ev)
                    }
                }
            }
        }
        .padding(10)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
        .contentShape(RoundedRectangle(cornerRadius: 12))
        .onTapGesture { selectedFeature = feature(from: ev) }
    }

    /// Compact thumbnail-first card for grid mode. Wraps `CameraFeedView` so
    /// the same resolver runs (live YouTube / image / snapshot fallback) but
    /// without the inline metadata footer the list card carries.
    private func gridCard(_ ev: CameraEvent) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            CameraFeedView(
                directSnapshotURLString: ev.snapshot_url,
                pageURLString: ev.url,
                youtubeID: ev.propString("youtube_id"),
                hlsURLString: ev.propString("hls_url"),
                discoveryChannel: ev.firstDiscoveryChannel,
                cameraUID: ev.id,
                originalPageURLString: ev.propString("original_page_url"),
                style: .compact,
                showsHeader: false
            )
            VStack(alignment: .leading, spacing: 2) {
                Text(ev.title ?? ev.id)
                    .font(.caption.bold())
                    .foregroundStyle(theme.text)
                    .lineLimit(2, reservesSpace: true)
                if let ch = ev.firstDiscoveryChannel {
                    Text(ch)
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(8)
        }
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
        .contentShape(RoundedRectangle(cornerRadius: 12))
        .onTapGesture { selectedFeature = feature(from: ev) }
    }

    private var emptyState: some View {
        OfflineStateView(
            kind: .empty,
            title: "No cameras stored yet.",
            message: "Tap “Re-run discovery” to scan now.",
            systemImage: "video.slash"
        )
    }

    private var noMatchView: some View {
        Text(searchText.isEmpty
             ? "No cameras match the current filters."
             : "No cameras match \"\(searchText)\"")
            .font(.caption)
            .foregroundStyle(theme.textMuted)
            .padding()
    }

    private func iconButton(
        systemName: String,
        tint: Color,
        accessibility: String,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            Image(systemName: systemName)
                .font(.body)
                .frame(width: 28, height: 28)
                .foregroundStyle(tint)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(accessibility)
    }

    // MARK: - Filtering

    private var filtersAreActive: Bool {
        !selectedChannels.isEmpty || feedAvailability != .all
    }

    /// Apply search + source-channel + feed-availability filters. Search hay
    /// includes channel + title + URL so users can pivot from the filter
    /// chips back to a free-text query without losing matches.
    private var filteredEvents: [CameraEvent] {
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        return events.filter { ev in
            if !selectedChannels.isEmpty {
                guard let ch = ev.firstDiscoveryChannel,
                      selectedChannels.contains(ch) else { return false }
            }
            switch feedAvailability {
            case .all:  break
            case .has:  if !eventHasFeed(ev) { return false }
            case .none: if eventHasFeed(ev)  { return false }
            }
            if !q.isEmpty {
                let hay = [ev.title, ev.url, ev.id, ev.kind, ev.snapshot_url, ev.firstDiscoveryChannel]
                    .compactMap { $0 }
                    .joined(separator: " ")
                    .lowercased()
                if !hay.contains(q) { return false }
            }
            return true
        }
    }

    /// Mirrors `CameraFeedView`'s render decision so the "Has feed" filter
    /// agrees with what the cards will actually show. Calls the same resolver
    /// the view uses; `.linkOnly` is the only mode that produces the "No
    /// feed" placeholder.
    private func eventHasFeed(_ ev: CameraEvent) -> Bool {
        let m = CameraFeedResolver.resolve(
            directHint: ev.snapshot_url,
            pageHint: ev.url,
            youtubeID: ev.propString("youtube_id"),
            hlsHint: ev.propString("hls_url"),
            discoveryChannel: ev.firstDiscoveryChannel,
            cameraUID: ev.id
        )
        if case .linkOnly = m { return false }
        return true
    }

    private var availableChannels: [(channel: String, count: Int)] {
        var counts: [String: Int] = [:]
        for ev in events {
            guard let ch = ev.firstDiscoveryChannel else { continue }
            counts[ch, default: 0] += 1
        }
        return counts.map { ($0.key, $0.value) }
            .sorted { $0.count == $1.count ? $0.channel < $1.channel : $0.count > $1.count }
    }

    private func toggleChannel(_ ch: String) {
        if selectedChannels.contains(ch) {
            selectedChannels.remove(ch)
        } else {
            selectedChannels.insert(ch)
        }
    }

    // MARK: - Filters sheet

    private var filtersSheet: some View {
        NavigationStack {
            Form {
                Section("Feed availability") {
                    Picker("Feed availability", selection: $feedAvailability) {
                        ForEach(FeedAvailability.allCases) { fa in
                            Text(fa.label).tag(fa)
                        }
                    }
                    .pickerStyle(.segmented)
                }
                Section("Camera source") {
                    if availableChannels.isEmpty {
                        Text("No sources yet")
                            .foregroundStyle(theme.textMuted)
                    } else {
                        ForEach(availableChannels, id: \.channel) { entry in
                            Button { toggleChannel(entry.channel) } label: {
                                HStack {
                                    Text(entry.channel)
                                        .foregroundStyle(theme.text)
                                    Spacer()
                                    Text("\(entry.count)")
                                        .font(.caption2.monospaced())
                                        .foregroundStyle(theme.textMuted)
                                    if selectedChannels.contains(entry.channel) {
                                        Image(systemName: "checkmark")
                                            .foregroundStyle(theme.accent)
                                    }
                                }
                            }
                        }
                    }
                }
                if filtersAreActive {
                    Section {
                        Button("Reset filters", role: .destructive) {
                            selectedChannels.removeAll()
                            feedAvailability = .all
                        }
                    }
                }
            }
            .navigationTitle("Filters")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { showFilters = false }
                }
            }
        }
        .presentationDetents([.medium, .large])
    }

    // MARK: - Feature / saved bridges (unchanged)

    /// Build a synthetic GeoFeature from a CameraEvent so the existing
    /// `featurePopup(for:)` dispatcher can render the same CameraPopup the
    /// map uses.
    private func feature(from ev: CameraEvent) -> GeoFeature {
        var props: [String: AnyCodable] = ev.properties ?? [:]
        if props["name"] == nil, let t = ev.title              { props["name"] = AnyCodable(t) }
        if props["snapshot_url"] == nil, let s = ev.snapshot_url { props["snapshot_url"] = AnyCodable(s) }
        if props["url"] == nil, let u = ev.url                 { props["url"] = AnyCodable(u) }
        if props["timestamp"] == nil, let ts = ev.timestamp    { props["timestamp"] = AnyCodable(ts) }
        let coord = (ev.lat != nil && ev.lon != nil)
            ? CLLocationCoordinate2D(latitude: ev.lat!, longitude: ev.lon!)
            : CLLocationCoordinate2D(latitude: 0, longitude: 0)
        return GeoFeature(
            id: ev.id,
            layerId: "camera-discovery",
            geometry: .point(coord),
            properties: props
        )
    }

    private func savedId(_ ev: CameraEvent) -> String {
        "camera-discovery|\(ev.id)"
    }

    private func toggleSaved(_ ev: CameraEvent) {
        let id = savedId(ev)
        if saved.contains(id: id) {
            saved.remove(id: id)
            return
        }
        guard let lat = ev.lat, let lon = ev.lon else { return }
        var props: [String: AnyCodable] = [:]
        if let t = ev.title         { props["title"] = AnyCodable(t) }
        if let s = ev.snapshot_url  { props["snapshot_url"] = AnyCodable(s) }
        if let u = ev.url           { props["url"] = AnyCodable(u) }
        if let ts = ev.timestamp    { props["timestamp"] = AnyCodable(ts) }
        let item = SavedItem(
            id: id,
            layerId: "camera-discovery",
            displayName: ev.title ?? ev.id,
            lat: lat, lon: lon,
            imageURL: ev.snapshot_url,
            properties: props,
            savedAt: Date()
        )
        saved.add(item)
    }

    private func badgeColor(_ kind: String) -> Color {
        switch kind.lowercased() {
        case "new":     return theme.success
        case "updated": return theme.warning
        default:        return theme.accent
        }
    }

    // MARK: - WS + REST seed (unchanged)

    private func subscribe() {
        subscription = ws.cameras.sink { ev in
            Task { @MainActor in
                events.removeAll { $0.id == ev.id }
                events.append(ev)
                // Mark the camera as "live" for this session so the NEW
                // badge survives even if the user reorders / filters.
                liveIds.insert(ev.id)
                if events.count > 2000 {
                    events.removeFirst(events.count - 2000)
                }
            }
        }
    }

    /// Backfill from the discovery-feed endpoint. Pulls historical events
    /// (record_type IN ('camera','camera-discovery') across every channel),
    /// not just the dedup'd fused set — so the tab opens populated with
    /// thousands of past discoveries instead of just the most recent sweep.
    private func seed() async {
        guard !seeded else { return }
        seeded = true
        do {
            let result = try await API(baseURL: settings.backendBaseURL)
                .cameraDiscoveryFeed(limit: 1000)
            await MainActor.run {
                // Live events that landed before the seed finished take
                // precedence — dedup the backfill against them by id.
                let already = Set(events.map(\.id))
                let merged = result.events.filter { !already.contains($0.id) } + events
                let trimmed = merged.suffix(2000)
                events = Array(trimmed)
                feedCursor = result.cursor
            }
        } catch {
            seeded = false
        }
    }

    /// Page older history when the user scrolls past the seeded window.
    private func loadMore() async {
        guard !loadingMore, let cursor = feedCursor else { return }
        loadingMore = true
        defer { loadingMore = false }
        do {
            let result = try await API(baseURL: settings.backendBaseURL)
                .cameraDiscoveryFeed(limit: 1000, cursor: cursor)
            await MainActor.run {
                let already = Set(events.map(\.id))
                let older = result.events.filter { !already.contains($0.id) }
                let merged = older + events
                let trimmed = merged.suffix(2000)
                events = Array(trimmed)
                feedCursor = result.cursor
            }
        } catch { /* keep cursor so user can retry */ }
    }

    private func trigger() async {
        triggering = true
        defer { triggering = false }
        do {
            try await API(baseURL: settings.backendBaseURL).triggerCameraDiscovery()
            triggerError = nil
        } catch {
            triggerError = error.localizedDescription
        }
    }
}
