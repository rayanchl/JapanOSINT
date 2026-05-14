import SwiftUI
import MapKit
import CoreLocation

struct FeaturePopup: View {
    let feature: GeoFeature
    var showsMiniMap: Bool = false
    @EnvironmentObject var saved: SavedStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                header
                if isApproximateTime { approximateTimeBadge }

                if let urlStr = feature.imageURL, let url = URL(string: urlStr) {
                    AsyncImage(url: url) { phase in
                        switch phase {
                        case .empty:    ProgressView().frame(height: 160)
                        case .success(let img):
                            img.resizable()
                               .scaledToFill()
                               .frame(maxWidth: .infinity, maxHeight: 200)
                               .clipShape(RoundedRectangle(cornerRadius: 10))
                        case .failure:  EmptyView()
                        @unknown default: EmptyView()
                        }
                    }
                }

                if !feature.properties.isEmpty {
                    PopupSectionHeader("Properties", icon: "list.bullet.rectangle")
                    propertiesGrid
                }

                if let coord = feature.geometry.anchor {
                    PopupSectionHeader("Coordinates", icon: "mappin.and.ellipse")
                    if showsMiniMap {
                        CoordinateMiniMap(coordinate: coord)
                    }
                    CoordinateAddressView(coordinate: coord)
                }

                if let urlStr = feature.externalLink, let url = URL(string: urlStr) {
                    Link(destination: url) {
                        Label("Open source", systemImage: "safari")
                    }
                    .buttonStyle(.borderedProminent)
                }
            }
            .padding()
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(feature.displayName)
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

    /// True when the server rendered this row via the COALESCE fallback to
    /// `fetched_at` because the layer's primary event-time column was NULL.
    /// Time-slider replay tags those rows so we can flag them as approximate.
    private var isApproximateTime: Bool {
        if let v = feature.properties["approx_time"]?.value as? Bool { return v }
        if let n = feature.properties["approx_time"]?.value as? NSNumber { return n.boolValue }
        return false
    }

    private var approximateTimeBadge: some View {
        HStack(spacing: 6) {
            Image(systemName: "clock.badge.questionmark")
                .font(.caption.weight(.semibold))
            VStack(alignment: .leading, spacing: 1) {
                Text("Approximate time")
                    .font(.caption.weight(.semibold))
                Text("Event time missing; placed by fetch time.")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
        }
        .foregroundStyle(theme.textMuted)
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private var propertiesGrid: some View {
        VStack(spacing: 1) {
            ForEach(sortedKeys, id: \.self) { key in
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

    private var sortedKeys: [String] {
        feature.properties.keys.sorted()
    }

    private func stringify(_ value: Any?) -> String {
        switch value {
        case nil:                 return "—"
        case let v as String:     return v
        case let v as NSNumber:   return v.stringValue
        case let v as Bool:       return v ? "true" : "false"
        case let v as Int:        return String(v)
        case let v as Double:     return String(v)
        case let v as [Any]:      return "[\(v.count) items]"
        case let v as [String: Any]: return "{\(v.count) keys}"
        default:                  return String(describing: value!)
        }
    }
}
