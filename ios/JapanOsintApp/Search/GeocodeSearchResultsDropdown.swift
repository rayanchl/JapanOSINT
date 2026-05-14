import SwiftUI
import MapKit

/// Floating results panel for `GeocodeSearchModel`. Hosted as an overlay on
/// `MapTab.searchRow` (not on the input field) so it spans the full row width
/// — the input alone is too narrow to lay out long Japanese display names
/// without truncating to "の丸公園, 千代田区,…".
struct GeocodeSearchResultsDropdown: View {
    @ObservedObject var model: GeocodeSearchModel
    let onPick: (CLLocationCoordinate2D) -> Void
    let onPickFeature: (GeoFeature) -> Void

    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    /// Cap on the dropdown's vertical extent. Keeps it from running off-screen
    /// when both feature and place hits are present; rows scroll inside.
    private static let dropdownMaxHeight: CGFloat = 320

    /// Vertical offset from the top of the host (the search row). 36 = search
    /// field height, 4 = breathing room.
    private static let dropdownTopOffset: CGFloat = 36 + 4

    var body: some View {
        resultsList
            .frame(maxWidth: .infinity)
            .background(.regularMaterial,
                        in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .strokeBorder(Color.primary.opacity(0.08), lineWidth: 0.5)
            )
            .shadow(color: .black.opacity(0.18), radius: 12, y: 6)
            .offset(y: Self.dropdownTopOffset)
            .transition(.opacity.combined(with: .move(edge: .top)))
    }

    private var resultsList: some View {
        ScrollView {
            VStack(spacing: 0) {
                if !model.featureHits.isEmpty {
                    sectionHeader("On map")
                    ForEach(Array(model.featureHits.enumerated()), id: \.element.id) { idx, tagged in
                        featureRow(tagged.feature, viaTranslation: tagged.viaTranslation)
                        if idx < model.featureHits.count - 1 { rowDivider }
                    }
                }
                if !model.hits.isEmpty {
                    if !model.featureHits.isEmpty { sectionHeader("Places") }
                    ForEach(Array(model.hits.enumerated()), id: \.element.id) { idx, hit in
                        placeRow(hit)
                        if idx < model.hits.count - 1 { rowDivider }
                    }
                }
            }
        }
        .frame(maxHeight: Self.dropdownMaxHeight)
        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
    }

    private var rowDivider: some View {
        Divider().padding(.leading, 12)
    }

    private func sectionHeader(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.caption2.bold())
            .foregroundStyle(theme.textMuted)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surfaceElevated.opacity(0.6))
    }

    private func placeRow(_ hit: GeocodeHit) -> some View {
        Button {
            onPick(hit.coordinate)
            model.query = hit.display_name ?? model.query
            model.showResults = false
        } label: {
            VStack(alignment: .leading, spacing: 4) {
                Text(hit.display_name ?? "—")
                    .font(.subheadline)
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                HStack(spacing: 6) {
                    if let s = hit.source {
                        Text(s.uppercased())
                            .font(.caption2.bold())
                            .padding(.horizontal, 5).padding(.vertical, 2)
                            .background(theme.accent.opacity(0.2), in: Capsule())
                            .foregroundStyle(theme.accent)
                    }
                    if hit.via_translation == true {
                        BilingualBadge(style: .compact)
                    }
                    Text(String(format: "%.4f, %.4f", hit.lat, hit.lon))
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }

    private func featureRow(_ feat: GeoFeature, viaTranslation: Bool) -> some View {
        let coord = feat.geometry.anchor ?? feat.geometry.centroid
        let layerLabel = registry.layer(for: feat.layerId)?.name ?? feat.layerId
        return Button {
            onPickFeature(feat)
            model.query = feat.displayName
            model.showResults = false
        } label: {
            VStack(alignment: .leading, spacing: 4) {
                Text(feat.displayName)
                    .font(.subheadline)
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                HStack(spacing: 6) {
                    Text(layerLabel.uppercased())
                        .font(.caption2.bold())
                        .padding(.horizontal, 5).padding(.vertical, 2)
                        .background(theme.accent.opacity(0.2), in: Capsule())
                        .foregroundStyle(theme.accent)
                    if viaTranslation {
                        BilingualBadge(style: .compact)
                    }
                    if let coord {
                        Text(String(format: "%.4f, %.4f", coord.latitude, coord.longitude))
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
