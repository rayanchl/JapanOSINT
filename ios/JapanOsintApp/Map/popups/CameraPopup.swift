import SwiftUI
import CoreLocation

/// Camera-flavoured popup. Pulls the relevant fields off the feature
/// (`thumbnail_url`, `url`, `youtube_id`, `discovery_channels`, …) and hands
/// them to `CameraFeedView`, which uses `CameraFeedResolver` to pick the right
/// rendering mode (direct image, YouTube embed, iframe, snapshot, HLS, proxy).
struct CameraPopup: View {
    let feature: GeoFeature
    var showsMiniMap: Bool = false
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var saved: SavedStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                header
                PopupSectionHeader("Camera feed", icon: "video.fill")
                CameraFeedView(
                    directSnapshotURLString: fields.directSnapshotURL,
                    pageURLString: fields.pageURL,
                    youtubeID: fields.youtubeID,
                    hlsURLString: fields.hlsURL,
                    discoveryChannel: fields.discoveryChannel,
                    cameraUID: fields.cameraUID,
                    originalPageURLString: fields.originalPageURL,
                    style: .full,
                    showsHeader: true
                )
                if let pageURL {
                    Link(destination: pageURL) {
                        Label("Open camera page", systemImage: "safari")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                }
                if !metadataKeys.isEmpty {
                    PopupSectionHeader("Properties", icon: "list.bullet.rectangle")
                    metadataGrid
                }
                coordinatesSection
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle("Camera")
        .compatInlineTitle()
        .toolbar {
            ToolbarItem(placement: .compatLeading) {
                Button { saved.toggle(feature) } label: {
                    Image(systemName: saved.contains(id: feature.id) ? "star.fill" : "star")
                        .foregroundStyle(saved.contains(id: feature.id) ? theme.warning : theme.textMuted)
                }
                .accessibilityLabel(saved.contains(id: feature.id) ? "Remove from saved" : "Save")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button("Close") { dismiss() }
            }
        }
    }

    // MARK: - Header

    private var header: some View {
        BilingualHeader(feature: feature) {
            Text("·")
                .font(.title3)
                .foregroundStyle(theme.textMuted)
            Text(LayerRegistry.displayName(forId: feature.layerId))
                .font(.caption)
                .foregroundStyle(theme.textMuted)
                .lineLimit(1)
        }
    }

    // MARK: - Metadata + actions

    private var metadataGrid: some View {
        VStack(spacing: 1) {
            ForEach(metadataKeys, id: \.self) { key in
                HStack(alignment: .top, spacing: 8) {
                    Text(key)
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 110, alignment: .leading)
                    JapaneseAware(
                        text: stringify(feature.properties[key]?.value),
                        font: .caption,
                        foregroundStyle: AnyShapeStyle(theme.text)
                    )
                }
                .padding(8)
                .background(theme.surfaceElevated)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    @ViewBuilder
    private var coordinatesSection: some View {
        if let coord = feature.geometry.anchor {
            PopupSectionHeader("Coordinates", icon: "mappin.and.ellipse")
            if showsMiniMap {
                CoordinateMiniMap(coordinate: coord)
            }
            CoordinateAddressView(coordinate: coord)
        }
    }

    // MARK: - URL resolution

    /// Shared with `IntelDetail` — see `CameraFeedFields`.
    private var fields: CameraFeedFields {
        CameraFeedFields(properties: feature.properties)
    }

    private var pageURL: URL? {
        fields.pageURL.flatMap { URL(string: $0) }
    }

    private var metadataKeys: [String] {
        // Keys already surfaced elsewhere (header, feed image, snapshot URL)
        // or that are internal identifiers users don't care about.
        let consumed = CameraFeedFields.consumedKeys
            .union(["icon", "name", "name_ja", "title"])
        return feature.properties.keys.filter { !consumed.contains($0) }.sorted()
    }

    // MARK: - Helpers

    private func stringify(_ v: Any?) -> String {
        switch v {
        case nil:                 return "—"
        case let s as String:     return s
        case let b as Bool:       return b ? "true" : "false"
        case let n as Int:        return String(n)
        case let n as Double:     return String(n)
        case let n as NSNumber:   return n.stringValue
        default:                  return v.map { String(describing: $0) } ?? "—"
        }
    }
}
