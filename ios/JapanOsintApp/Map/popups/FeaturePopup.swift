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

                if !provenanceRows.isEmpty {
                    PopupSectionHeader("Provenance", icon: "shippingbox")
                    provenanceGrid
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
        .themedScreenBackground(theme)
        .navigationTitle(feature.displayName)
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

    /// Best-effort "info path" for a map feature. Map layers flow through a
    /// different pipeline than intel items (no server `provenance` block), so
    /// we assemble it from the layer registry + whatever source/fetch keys
    /// the collector left in `properties`. Only non-empty rows render.
    private var provenanceRows: [(String, String)] {
        func prop(_ keys: [String]) -> String? {
            for k in keys {
                if let v = feature.properties[k]?.value {
                    let s = stringify(v)
                    if !s.isEmpty, s != "—" { return s }
                }
            }
            return nil
        }
        var rows: [(String, String)] = [
            ("Layer", LayerRegistry.displayName(forId: feature.layerId)),
        ]
        if let s = prop(["source", "source_id", "collector", "provider", "operator"]) {
            rows.append(("Source", s))
        }
        if let s = prop(["source_url", "feed_url", "api", "endpoint"]) ?? feature.externalLink {
            rows.append(("Origin", s))
        }
        if let s = prop(["fetched_at", "fetchedAt", "retrieved_at", "ingested_at"]) {
            rows.append(("Fetched", s))
        }
        if let s = prop(["published_at", "timestamp", "time", "datetime", "date"]) {
            rows.append(("Reported", s))
        }
        if let s = prop(["license", "license_id", "license_name", "licence"]) {
            rows.append(("License", s))
        }
        return rows
    }

    private var provenanceGrid: some View {
        VStack(spacing: 1) {
            ForEach(provenanceRows.indices, id: \.self) { i in
                let (label, value) = provenanceRows[i]
                HStack(alignment: .top, spacing: 8) {
                    Text(label)
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 90, alignment: .leading)
                    if (label == "Origin"), let url = URL(string: value), url.scheme != nil {
                        Link(value, destination: url)
                            .font(.caption.monospaced())
                            .foregroundStyle(theme.accentAlt)
                            .lineLimit(2)
                    } else {
                        Text(value)
                            .font(.caption.monospaced())
                            .foregroundStyle(theme.text)
                            .textSelection(.enabled)
                    }
                    Spacer(minLength: 0)
                }
                .padding(8)
                .background(theme.surfaceElevated)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
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
        default:                  return value.map { String(describing: $0) } ?? "—"
        }
    }
}
