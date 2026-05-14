import SwiftUI

struct StationPopup: View {
    let feature: GeoFeature
    var showsMiniMap: Bool = false
    @EnvironmentObject var saved: SavedStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                BilingualHeader(feature: feature) {
                    Text("·").font(.title3).foregroundStyle(theme.textMuted)
                    Text(LayerRegistry.displayName(forId: feature.layerId))
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                }

                if hasProperties {
                    PopupSectionHeader("Properties", icon: "list.bullet.rectangle")
                    if let line = feature.properties["line"]?.value as? String {
                        chip("Line", value: line)
                    }
                    if let op = feature.properties["operator"]?.value as? String {
                        chip("Operator", value: op)
                    }
                    if let code = feature.properties["station_code"]?.value as? String {
                        chip("Code", value: code)
                    }
                }

                if let coord = feature.geometry.anchor {
                    PopupSectionHeader("Coordinates", icon: "mappin.and.ellipse")
                    if showsMiniMap {
                        CoordinateMiniMap(coordinate: coord)
                    }
                    CoordinateAddressView(coordinate: coord)
                }

                Divider().padding(.vertical, 4)

                DeparturesBoard(stationKey: stationKey)
            }
            .padding()
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Station")
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

    private var hasProperties: Bool {
        feature.properties["line"] != nil
            || feature.properties["operator"] != nil
            || feature.properties["station_code"] != nil
    }

    private var stationKey: String {
        // Best-effort key the backend can resolve; falls back to feature id.
        if let s = feature.properties["station_id"]?.value as? String { return s }
        if let s = feature.properties["id"]?.value as? String          { return s }
        return feature.id
    }

    private func chip(_ label: String, value: String) -> some View {
        HStack {
            Text(label)
                .font(.caption.bold())
                .foregroundStyle(theme.textMuted)
            Spacer()
            JapaneseAware(
                text: value,
                font: .caption,
                foregroundStyle: AnyShapeStyle(theme.text),
                alignment: .trailing
            )
                .layoutPriority(0)
        }
        .padding(8)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
    }
}
