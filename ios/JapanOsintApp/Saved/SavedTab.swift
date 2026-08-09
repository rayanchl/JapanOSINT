import SwiftUI
import MapKit

struct SavedTab: View {
    @EnvironmentObject var saved: SavedStore
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme
    @Environment(\.openURL) private var openURL
    @Environment(\.dismiss) private var dismiss

    @State private var mode: Mode = .list
    @State private var columns: Int = 2
    @State private var selectedFeature: GeoFeature?
    @State private var cameraPosition: MapCameraPosition = SavedTab.japanRegion

    @State private var searchText: String = ""
    @State private var imagePresence: ImagePresence = .all
    @State private var selectedLayers: Set<String> = []
    @State private var showFilters: Bool = false

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

    enum ImagePresence: String, FilterChoice {
        case all, withImage, noImage
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:       return "All"
            case .withImage: return "With image"
            case .noImage:   return "No image"
            }
        }
    }

    /// NOTE: this view does NOT own a `NavigationStack`, exactly like
    /// `CameraDiscoveryView`. Saved is no longer a phone tab — it is pushed
    /// from Console (`ConsoleHub`'s `navigationDestination`) and is a sidebar
    /// detail on iPad, so both of those already supply a stack. Adding another
    /// one here nests a NavigationStack inside a `navigationDestination`, which
    /// SwiftUI does not support: the title/search chrome binds to the inner
    /// stack while the outer stack still owns the path, and the two fight over
    /// the same update. Callers that have no stack of their own (the iPad
    /// sidebar detail) wrap it — see `RootView.detailView`.
    var body: some View {
        Group {
            switch mode {
            case .list: listView
            case .grid: gridView
            case .map:  mapWithPicker
            }
        }
        .themedScreenBackground(theme)
        // Blank the large title in map mode and switch to inline so the
        // map gets the full viewport. The previous approach (.toolbar(.hidden)
        // + a floating chevron overlay) pulled SwiftUI into a toolbar-
        // transition cycle that blocked the body rebuild, so the mode
        // picker visually flipped segments without swapping the view.
        .navigationTitle(mode == .map ? "" : "Saved")
        .compatInlineTitle(mode == .map)
        .searchable(
            text: $searchText,
            placement: .compatDrawer,
            prompt: "Search saved"
        )
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                FilterToolbarButton(isActive: filtersAreActive) {
                    showFilters = true
                }
            }
        }
        .sheet(item: $selectedFeature) { feat in
            NavigationStack { featurePopup(for: feat, showsMiniMap: true) }
                .compatSheetSizing(.medium)
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
    }

    // MARK: - Pickers

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
                Text("4").tag(4)
            }
            .pickerStyle(.segmented)
            .frame(maxWidth: 180)
        }
        .padding(.horizontal)
        .padding(.bottom, 8)
    }

    // MARK: - List (picker scrolls with content so the large title can collapse)

    private var listView: some View {
        List {
            Section {
                modePicker
                    .listRowBackground(Color.clear)
                    // Horizontal insets are 0 here because `modePicker` already
                    // applies `.padding(.horizontal)`. Stacking another 16pt of
                    // row inset on top makes the pill narrower in List mode
                    // than in Grid/Map and produces visible width jitter on
                    // mode toggle.
                    .listRowInsets(EdgeInsets(top: 0, leading: 0, bottom: 8, trailing: 0))
                    .listRowSeparator(.hidden)
            }
            if saved.items.isEmpty {
                emptyState
                    .frame(maxWidth: .infinity)
                    .listRowBackground(Color.clear)
                    .listRowSeparator(.hidden)
            } else if filteredItems.isEmpty {
                noMatchView
                    .frame(maxWidth: .infinity)
                    .listRowBackground(Color.clear)
                    .listRowSeparator(.hidden)
            } else {
                ForEach(filteredItems) { item in
                    Button { selectedFeature = item.toFeature() } label: {
                        SavedListRow(item: item)
                    }
                    .buttonStyle(.plain)
                    .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                        Button(role: .destructive) {
                            saved.remove(id: item.id)
                        } label: {
                            Label("Unfavorite", systemImage: "star.slash")
                        }
                    }
                    .contextMenu { contextActions(for: item) }
                }
            }
        }
        .listStyle(.plain)
    }

    // MARK: - Grid (pickers ride inside the ScrollView so the title can collapse)

    private var gridView: some View {
        ScrollView {
            VStack(spacing: 0) {
                modePicker
                columnsPicker
                if saved.items.isEmpty {
                    emptyState
                        .frame(maxWidth: .infinity)
                        .padding(.top, 40)
                } else if filteredItems.isEmpty {
                    noMatchView
                        .frame(maxWidth: .infinity)
                        .padding(.top, 40)
                } else {
                    LazyVGrid(columns: gridColumns, spacing: 10) {
                        ForEach(filteredItems) { item in
                            SavedCard(item: item)
                                .contentShape(Rectangle())
                                .onTapGesture {
                                    Haptics.selection()
                                    selectedFeature = item.toFeature()
                                }
                                .contextMenu { contextActions(for: item) }
                        }
                    }
                    .padding(.horizontal)
                    .padding(.bottom, 10)
                }
            }
        }
    }

    // MARK: - Map

    private var mapWithPicker: some View {
        VStack(spacing: 0) {
            modePicker
            mapView
        }
    }

    private var gridColumns: [GridItem] {
        Array(repeating: GridItem(.flexible(), spacing: 10), count: columns)
    }

    // MARK: - Context menu actions (shared by list + grid)

    @ViewBuilder
    private func contextActions(for item: SavedItem) -> some View {
        Button { selectedFeature = item.toFeature() } label: {
            Label("Open in map", systemImage: "map")
        }
        Button { openInAppleMaps(item) } label: {
            Label("Open in Apple Maps", systemImage: "location.fill")
        }
        ShareLink(item: shareString(item)) {
            Label("Share", systemImage: "square.and.arrow.up")
        }
        if let url = externalURL(for: item) {
            Button { openURL(url) } label: {
                Label("Open link", systemImage: "safari")
            }
        }
        Divider()
        Button(role: .destructive) {
            saved.remove(id: item.id)
        } label: {
            Label("Unfavorite", systemImage: "star.slash")
        }
    }

    private func externalURL(for item: SavedItem) -> URL? {
        guard let raw = item.toFeature().externalLink else { return nil }
        return URL(string: raw)
    }

    private func shareString(_ item: SavedItem) -> String {
        var lines = [item.displayName, "\(item.lat), \(item.lon)"]
        if let raw = item.toFeature().externalLink { lines.append(raw) }
        return lines.joined(separator: "\n")
    }

    private func openInAppleMaps(_ item: SavedItem) {
        let location = CLLocation(latitude: item.coordinate.latitude,
                                  longitude: item.coordinate.longitude)
        let mapItem = MKMapItem(location: location, address: nil)
        mapItem.name = item.displayName
        mapItem.openInMaps()
    }

    // MARK: - Map

    @ViewBuilder
    private var mapView: some View {
        Map(position: $cameraPosition) {
            ForEach(filteredItems) { item in
                MapKit.Annotation(item.displayName, coordinate: item.coordinate, anchor: .bottom) {
                    Button {
                        selectedFeature = item.toFeature()
                    } label: {
                        mapPinView(
                            symbol: registry.symbol(for: item.layerId),
                            color: registry.color(for: item.layerId)
                        )
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
        .onChange(of: filteredItems.count) { _, _ in fitToFiltered() }
        .overlay(alignment: .center) {
            if saved.items.isEmpty {
                Text("No saved items yet — pin features from the map to see them here.")
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .multilineTextAlignment(.center)
                    .padding(10)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                    .padding(.horizontal, 32)
            } else if filteredItems.isEmpty {
                Text("No saved items match the current filters.")
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .multilineTextAlignment(.center)
                    .padding(10)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                    .padding(.horizontal, 32)
            }
        }
    }

    /// Always-on Japan default; auto-fit only when items exist. Bounded over
    /// the *filtered* set so applying a filter retargets the camera instead of
    /// staying anchored on the full collection.
    private func fitToFiltered() {
        guard !filteredItems.isEmpty else {
            cameraPosition = SavedTab.japanRegion
            return
        }
        let lats = filteredItems.map(\.lat)
        let lons = filteredItems.map(\.lon)
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

    // MARK: - Filtering

    private var filtersAreActive: Bool {
        !selectedLayers.isEmpty || imagePresence != .all
    }

    /// Apply search + layer + image-presence filters. Search hay includes the
    /// raw layer ID and the human-readable layer name so a query like
    /// "camera" matches camera-discovery items even when the displayName
    /// doesn't contain the word.
    private var filteredItems: [SavedItem] {
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        return saved.items.filter { item in
            if !selectedLayers.isEmpty, !selectedLayers.contains(item.layerId) {
                return false
            }
            switch imagePresence {
            case .all:
                break
            case .withImage:
                if (item.imageURL?.isEmpty ?? true) { return false }
            case .noImage:
                if !(item.imageURL?.isEmpty ?? true) { return false }
            }
            if !q.isEmpty {
                let hay = [
                    item.displayName,
                    item.layerId,
                    LayerRegistry.displayName(forId: item.layerId),
                ]
                .joined(separator: " ")
                .lowercased()
                if !hay.contains(q) { return false }
            }
            return true
        }
    }

    /// Layer chips for the filter sheet. Sorted by descending count so the
    /// most-saved layers float to the top, with alphabetical tie-break.
    private var availableLayers: [(layerId: String, count: Int)] {
        var counts: [String: Int] = [:]
        for item in saved.items {
            counts[item.layerId, default: 0] += 1
        }
        return counts.map { ($0.key, $0.value) }
            .sorted {
                $0.count == $1.count
                    ? $0.layerId < $1.layerId
                    : $0.count > $1.count
            }
    }

    private func toggleLayer(_ layerId: String) {
        if selectedLayers.contains(layerId) {
            selectedLayers.remove(layerId)
        } else {
            selectedLayers.insert(layerId)
        }
    }

    // MARK: - Filters sheet

    private var filtersSheet: some View {
        FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                selectedLayers.removeAll()
                imagePresence = .all
            },
            onDone: { showFilters = false }
        ) {
            FilterSegmentedSection(title: "Image", selection: $imagePresence)
            FilterMultiSelectSection(
                title: "Layer",
                items: availableLayers.map(\.layerId),
                emptyText: "No saved items yet",
                label: { LayerRegistry.displayName(forId: $0) },
                symbol: { (registry.symbol(for: $0), registry.color(for: $0)) },
                count: { id in availableLayers.first { $0.layerId == id }?.count ?? 0 },
                isSelected: { selectedLayers.contains($0) },
                toggle: { toggleLayer($0) }
            )
        }
    }

    // MARK: - Empty state

    private var emptyState: some View {
        OfflineStateView(
            kind: .empty,
            title: "No saved items yet",
            message: "Tap the star icon on any feature popup or camera card to save it here.",
            systemImage: "star.slash"
        )
    }

    private var noMatchView: some View {
        // HIG-native "no results" state; themed to match the palette.
        ContentUnavailableView {
            Label("No matches", systemImage: "line.3.horizontal.decrease.circle")
                .foregroundStyle(theme.text)
        } description: {
            Text(searchText.isEmpty
                 ? "No saved items match the current filters."
                 : "No saved items match \"\(searchText)\"")
                .foregroundStyle(theme.textMuted)
        }
    }
}
