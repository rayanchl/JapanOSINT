import SwiftUI

struct LayersTab: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    @State private var searchText = ""
    @State private var collapsed: Set<String> = []

    var body: some View {
        NavigationStack {
            List {
                Section {
                    HStack {
                        Button("All on") {
                            Haptics.tap(.medium)
                            settings.enableAll(filteredLayers.map(\.id))
                        }
                        .buttonStyle(.bordered)
                        Button("All off", role: .destructive) {
                            Haptics.tap(.medium)
                            settings.disableAll()
                        }
                        .buttonStyle(.bordered)
                        Spacer()
                        Text("\(settings.activeLayerIds.count) active")
                            .font(.caption.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                    }
                }

                if !activeLayers.isEmpty {
                    Section("Active (\(activeLayers.count))") {
                        ForEach(activeLayers) { layer in
                            LayerRow(layer: layer)
                        }
                    }
                }

                if registry.isLoading && registry.layers.isEmpty {
                    Section { ProgressView("Loading layers…") }
                } else if let err = registry.lastError, registry.layers.isEmpty {
                    Section {
                        Text(err)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                }

                ForEach(grouped, id: \.category) { group in
                    Section {
                        if !collapsed.contains(group.category) {
                            ForEach(group.layers) { layer in
                                LayerRow(layer: layer)
                            }
                            // Pin the WebSocket-driven live vehicles toggle
                            // inside Transport so it sits next to the
                            // train/subway/bus layers it complements.
                            if group.category == "Transport" {
                                LiveVehiclesRow()
                            }
                        }
                    } header: {
                        Button {
                            toggleCollapse(group.category)
                        } label: {
                            HStack {
                                Image(systemName: collapsed.contains(group.category)
                                      ? "chevron.right" : "chevron.down")
                                Text(group.category)
                                    .font(.subheadline.bold())
                                Spacer()
                                Text("\(group.layers.count)")
                                    .font(.caption2.monospacedDigit())
                                    .foregroundStyle(theme.textMuted)
                            }
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            .listStyle(.insetGrouped)
            .searchable(text: $searchText, prompt: "Search layers")
            .navigationTitle("Layers (\(filteredLayers.count))")
            .refreshable {
                await registry.reload(baseURL: settings.backendBaseURL)
            }
        }
    }

    /// Visible layers come straight from `registry.layers` — the server-side
    /// `STRIP_LAYER_IDS` filter (routes/layers.js) already removed every
    /// ingredient, rollup, and UX-merge id, so iOS doesn't filter again.
    private var filteredLayers: [LayerDef] {
        let base = registry.layers
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        guard !q.isEmpty else { return base }
        return base.filter {
            $0.name.lowercased().contains(q) ||
            $0.id.lowercased().contains(q) ||
            ($0.category ?? "").lowercased().contains(q)
        }
    }

    /// Active layers shown above the category breakdown for quick access.
    /// Honors the search filter so it stays in sync with the rest of the list.
    private var activeLayers: [LayerDef] {
        filteredLayers
            .filter { settings.activeLayerIds.contains($0.id) }
            .sorted { $0.name < $1.name }
    }

    private var grouped: [(category: String, layers: [LayerDef])] {
        Dictionary(grouping: filteredLayers, by: \.categoryLabel)
            .map { ($0.key, $0.value.sorted { $0.name < $1.name }) }
            .sorted { lhs, rhs in
                let lr = Self.categoryRank(lhs.category)
                let rr = Self.categoryRank(rhs.category)
                if lr != rr { return lr < rr }
                return lhs.category < rhs.category
            }
    }

    /// Priority ordering for the category sections. Most relevant for an
    /// OSINT analyst (state security, surveillance, critical infrastructure)
    /// at the top; soft / cultural categories at the bottom. Unknown
    /// categories — including legacy `Other` and any new server-added
    /// category — land after the curated list and sort alphabetically
    /// among themselves so the output stays deterministic.
    private static let categoryPriority: [String] = [
        // Tier 1 — direct state security / surveillance
        "defense", "government", "safety", "intelligence", "cyber",
        // Tier 2 — strategic infrastructure / industry
        "infrastructure", "telecom", "industry", "satellite",
        // Tier 3 — movement & comms intelligence
        "transport", "social",
        // Tier 4 — disaster / public-order signals
        "environment", "crime", "wildlife",
        // Tier 5 — civilian baseline
        "health", "economy", "statistics",
        "marketplace", "classifieds", "commercial",
        // Tier 6 — geospatial / interest
        "geospatial",
        "food", "agriculture", "culture", "tourism",
    ]

    private static func categoryRank(_ category: String) -> Int {
        categoryPriority.firstIndex(of: category.lowercased())
            ?? categoryPriority.count
    }

    private func toggleCollapse(_ cat: String) {
        if collapsed.contains(cat) { collapsed.remove(cat) }
        else { collapsed.insert(cat) }
    }
}

struct LayerRow: View {
    let layer: LayerDef
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var registry: LayerRegistry
    @EnvironmentObject var nav: MapNavigation
    @EnvironmentObject var stats: FeatureStats
    @EnvironmentObject var playback: PlaybackState
    @Environment(\.theme) private var theme

    // Camera-discovery trigger state, only used when this row hosts the
    // `cameras` layer.
    @State private var triggering = false
    @State private var triggerError: String?
    @State private var lastTriggered: Date?

    @State private var isExpanded = false
    @State private var chevronToken: Int = 0

    var isActive: Bool { settings.activeLayerIds.contains(layer.id) }
    var hostsCameraDiscovery: Bool { layer.id == "cameras" }
    var followers: [String] { settings.followers(of: layer.id) }
    var hasFollowers: Bool { !followers.isEmpty }
    var hasLivePositionsToggle: Bool {
        layer.id == "unified-trains" || layer.id == "unified-subways" || layer.id == "unified-buses"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            header
            if isExpanded {
                expandedBody
            }
        }
        .padding(.vertical, 2)
        // No animation on expand: prevents the header (icon/title/switch)
        // from jittering as SwiftUI interpolates the row's height during
        // the expanded body's appearance.
        .animation(nil, value: isExpanded)
    }

    /// Single-line compact header: icon · name · source/feature counts ·
    /// dropdown chevron · activation toggle. Always-on chevron lets the
    /// user inspect any layer (sources list, etc.) without having to
    /// activate it first.
    private var header: some View {
        HStack(spacing: 10) {
            Image(systemName: registry.symbol(for: layer.id))
                .font(.title3)
                .foregroundStyle(registry.color(for: layer.id))
                .frame(width: 28, height: 28)

            HStack(spacing: 4) {
                Text(registry.displayName(for: layer))
                    .font(.subheadline)
                    .lineLimit(1)
                if let n = layer.sources?.count, n > 0 {
                    Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    Text("\(n)")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let count = stats.count(for: layer.id) {
                    Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    Text("\(count.formatted())")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if layer.isLiveOnly && playback.isReplaying {
                    Text("Live only · hidden")
                        .font(.caption2.weight(.medium))
                        .foregroundStyle(theme.textMuted)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 2)
                        .background(theme.surfaceElevated, in: Capsule())
                }
            }
            Spacer()
            chevronIndicator
            Toggle("", isOn: Binding(
                get: { isActive },
                set: { _ in settings.toggleLayer(layer.id) }
            ))
            .labelsHidden()
        }
        .contentShape(Rectangle())
        .onTapGesture {
            isExpanded.toggle()
            chevronToken += 1
        }
    }

    /// Static chevron — fades out then in (via SF Symbol replace transition)
    /// when `chevronToken` changes. Token is bumped explicitly on tap so the
    /// animation runs only here, not on the rest of the row.
    private var chevronIndicator: some View {
        Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
            .font(.caption.weight(.semibold))
            .foregroundStyle(theme.textMuted)
            .frame(width: 24, height: 24)
            .contentTransition(.symbolEffect(.replace))
            .animation(.easeInOut(duration: 0.2), value: chevronToken)
    }

    /// Order: sources first (always useful, even when layer is off), then
    /// active-only sections (features, then opacity at the bottom).
    @ViewBuilder
    private var expandedBody: some View {
        VStack(alignment: .leading, spacing: 12) {
            if let sources = layer.sources, !sources.isEmpty {
                sourcesSection(sources)
            }
            if isActive && (hasFollowers || hostsCameraDiscovery || hasLivePositionsToggle) {
                featuresSection
            }
            if isActive {
                opacitySection
            }
        }
        .padding(.top, 4)
        // Skip the implicit fade/scale that would otherwise animate the
        // Slider's thumb position from left edge → real value during expand.
        .transition(.identity)
    }

    // MARK: - Sections

    private var opacitySection: some View {
        VStack(alignment: .leading, spacing: 4) {
            sectionLabel("OPACITY")
            HStack(spacing: 6) {
                Image(systemName: "circle.lefthalf.filled")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                Slider(
                    value: Binding(
                        get: { settings.opacity(for: layer.id) },
                        set: { settings.setOpacity($0, for: layer.id) }
                    ),
                    in: 0...1
                )
                Text("\(Int(settings.opacity(for: layer.id) * 100))%")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .monospacedDigit()
                    .frame(width: 36, alignment: .trailing)
            }
        }
    }

    private var featuresSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            sectionLabel("FEATURES")
            ForEach(followers, id: \.self) { followerId in
                Toggle(isOn: Binding(
                    get: { settings.followerEnabled(followerId) },
                    set: { _ in settings.toggleFollowerOptOut(followerId, parentId: layer.id) }
                )) {
                    HStack(spacing: 6) {
                        Image(systemName: registry.symbol(for: followerId))
                            .font(.caption)
                            .foregroundStyle(theme.accent)
                            .frame(width: 18)
                        Text("Show \(LayerRegistry.displayName(forId: followerId).lowercased())")
                            .font(.caption)
                    }
                }
                .toggleStyle(.checkbox)
            }
            switch layer.id {
            case "unified-trains":
                livePositionsToggle(
                    isOn: $settings.liveTrainsEnabled,
                    symbol: "tram.fill",
                    title: "Show train live positions"
                )
            case "unified-subways":
                livePositionsToggle(
                    isOn: $settings.liveSubwaysEnabled,
                    symbol: "tram.fill",
                    title: "Show subway live positions"
                )
            case "unified-buses":
                livePositionsToggle(
                    isOn: $settings.liveBusesEnabled,
                    symbol: "bus.fill",
                    title: "Show bus live positions"
                )
            default:
                EmptyView()
            }
            if hostsCameraDiscovery {
                cameraDiscoverySection
            }
        }
    }

    @ViewBuilder
    private func livePositionsToggle(
        isOn: Binding<Bool>,
        symbol: String,
        title: String
    ) -> some View {
        Toggle(isOn: isOn) {
            HStack(spacing: 6) {
                Image(systemName: symbol)
                    .font(.caption)
                    .foregroundStyle(theme.accent)
                    .frame(width: 18)
                Text(title).font(.caption)
            }
        }
        .toggleStyle(.checkbox)
    }

    private func sourcesSection(_ sources: [LayerSourceRef]) -> some View {
        let bySource = stats.sources(for: layer.id) ?? [:]
        return VStack(alignment: .leading, spacing: 6) {
            sectionLabel("SOURCES (\(sources.count))")
            VStack(spacing: 1) {
                ForEach(sources) { src in
                    Button {
                        nav.showSource(src.id)
                    } label: {
                        HStack(spacing: 6) {
                            Text(src.name ?? src.id)
                                .font(.caption)
                                .foregroundStyle(theme.text)
                                .lineLimit(1)
                            Spacer()
                            if let n = bySource[src.id] {
                                Text("\(n.formatted())")
                                    .font(.caption2.monospacedDigit().bold())
                                    .foregroundStyle(theme.text)
                                    .padding(.horizontal, 5)
                                    .padding(.vertical, 1)
                                    .background(registry.color(for: layer.id).opacity(0.18),
                                                in: Capsule())
                            }
                            if let t = src.type {
                                Text(t)
                                    .font(.caption2)
                                    .foregroundStyle(theme.textMuted)
                            }
                            if src.free == false {
                                Image(systemName: "key.fill")
                                    .font(.caption2)
                                    .foregroundStyle(theme.warning)
                            }
                            Image(systemName: "chevron.right")
                                .font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                        .padding(.vertical, 6)
                        .padding(.horizontal, 8)
                        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 6))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    private func sectionLabel(_ text: String) -> some View {
        Text(text)
            .font(.caption2.weight(.semibold))
            .foregroundStyle(theme.textMuted)
            .tracking(0.5)
    }

    /// Inline trigger for `/api/data/cameras/trigger`. Only rendered under the
    /// `cameras` layer when active so users have a single camera-related row.
    private var cameraDiscoverySection: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let last = lastTriggered {
                HStack {
                    Spacer()
                    Text("ran \(relative(last))")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            HStack {
                Button {
                    Task { await runDiscovery() }
                } label: {
                    HStack(spacing: 4) {
                        if triggering {
                            ProgressView().controlSize(.mini)
                        } else {
                            Image(systemName: "arrow.triangle.2.circlepath")
                        }
                        Text(triggering ? "Discovering…" : "Run discovery")
                    }
                    .font(.subheadline.weight(.medium))
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .disabled(triggering)
                Spacer()
            }
            if let err = triggerError {
                Text(err)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
                    .lineLimit(2)
            }
        }
        .padding(.top, 2)
    }

    private func runDiscovery() async {
        triggering = true
        triggerError = nil
        defer { triggering = false }
        do {
            try await API(baseURL: settings.backendBaseURL).triggerCameraDiscovery()
            lastTriggered = Date()
        } catch {
            triggerError = error.localizedDescription
        }
    }

    private func relative(_ d: Date) -> String {
        let f = RelativeDateTimeFormatter()
        f.unitsStyle = .abbreviated
        return f.localizedString(for: d, relativeTo: Date())
    }
}

/// Virtual layer row that drives the WebSocket-fed live vehicle annotations.
/// Lives in the same list as backend-defined layers so users have one mental
/// model for "what's on the map."
struct LiveVehiclesRow: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var ws: WebSocketClient
    @Environment(\.theme) private var theme

    var body: some View {
        HStack(spacing: 10) {
            ZStack {
                Circle()
                    .fill(theme.accentAlt.opacity(settings.liveCarriagesEnabled ? 0.85 : 0.25))
                    .frame(width: 28, height: 28)
                // Tram icon since this controls carriages (trains / subways /
                // buses) only — planes ride the unified-flights layer toggle
                // now and ships have no live source.
                Image(systemName: "tram.fill")
                    .font(.caption)
                    .foregroundStyle(.white)
            }

            VStack(alignment: .leading, spacing: 1) {
                Text("Live carriages")
                    .font(.subheadline)
                HStack(spacing: 4) {
                    Circle()
                        .fill(ws.isConnected ? theme.success : theme.danger)
                        .frame(width: 6, height: 6)
                    Text(ws.isConnected ? "WebSocket connected" : "WebSocket offline")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            Spacer()
            Toggle("", isOn: $settings.liveCarriagesEnabled)
                .labelsHidden()
        }
        .padding(.vertical, 2)
    }
}
