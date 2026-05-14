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
                    directSnapshotURLString: directSnapshotURLString,
                    pageURLString: pageURLString,
                    youtubeID: youtubeID,
                    hlsURLString: hlsURLString,
                    discoveryChannel: discoveryChannel,
                    cameraUID: cameraUID,
                    originalPageURLString: originalPageURLString,
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
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Camera")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarLeading) {
                Button { saved.toggle(feature) } label: {
                    Image(systemName: saved.contains(id: feature.id) ? "star.fill" : "star")
                        .foregroundStyle(saved.contains(id: feature.id) ? theme.warning : theme.textMuted)
                }
                .accessibilityLabel(saved.contains(id: feature.id) ? "Remove from saved" : "Save")
            }
            ToolbarItem(placement: .topBarTrailing) {
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

    private var directSnapshotURLString: String? {
        for key in ["thumbnail_url", "snapshot_url", "image", "thumbnail", "photo"] {
            if let raw = feature.properties[key]?.value as? String, !raw.isEmpty {
                return raw
            }
        }
        return nil
    }

    private var pageURL: URL? {
        pageURLString.flatMap { URL(string: $0) }
    }

    private var pageURLString: String? {
        for key in ["url", "source_url", "page_url", "embed_url", "link"] {
            if let raw = feature.properties[key]?.value as? String, !raw.isEmpty {
                return raw
            }
        }
        return nil
    }

    private var youtubeID: String? {
        (feature.properties["youtube_id"]?.value as? String).flatMap {
            $0.isEmpty ? nil : $0
        }
    }

    private var hlsURLString: String? {
        (feature.properties["hls_url"]?.value as? String).flatMap {
            $0.isEmpty ? nil : $0
        }
    }

    private var cameraUID: String? {
        (feature.properties["camera_uid"]?.value as? String).flatMap {
            $0.isEmpty ? nil : $0
        }
    }

    /// Original aggregator page URL — set by the server when the camera URL
    /// was upgraded (e.g. scs.com.ua → YouTube channel-live). Surfaced so
    /// `CameraFeedView` can fall back to a snapshot of this page when the
    /// upgraded iframe target fails to render.
    private var originalPageURLString: String? {
        (feature.properties["original_page_url"]?.value as? String).flatMap {
            $0.isEmpty ? nil : $0
        }
    }

    /// `discovery_channels` is JSON-encoded as either a `[String]` or a single
    /// string by the cameras GeoJSON serializer. Take the first entry.
    private var discoveryChannel: String? {
        if let arr = feature.properties["discovery_channels"]?.value as? [Any],
           let first = arr.first as? String, !first.isEmpty {
            return first
        }
        if let s = feature.properties["discovery_channels"]?.value as? String, !s.isEmpty {
            return s
        }
        if let s = feature.properties["discovery_channel"]?.value as? String, !s.isEmpty {
            return s
        }
        return nil
    }

    private var metadataKeys: [String] {
        // Keys already surfaced elsewhere (header, feed image, snapshot URL)
        // or that are internal identifiers users don't care about.
        let consumed: Set<String> = [
            "url", "source_url", "page_url", "embed_url", "link",
            "original_page_url",
            "snapshot_url", "thumbnail_url", "image", "thumbnail", "photo",
            "icon", "name", "name_ja", "title", "camera_uid",
            "youtube_id", "hls_url", "discovery_channels", "discovery_channel",
        ]
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
        default:                  return String(describing: v!)
        }
    }
}
