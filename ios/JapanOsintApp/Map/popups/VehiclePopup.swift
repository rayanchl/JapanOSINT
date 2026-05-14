import SwiftUI
import CoreLocation

struct VehiclePopup: View {
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

                PopupSectionHeader("Properties", icon: "list.bullet.rectangle")
                grid([
                    ("Callsign",  callsign),
                    ("Type",      stringProp("aircraft_type") ?? stringProp("vessel_type") ?? "—"),
                    ("Operator",  stringProp("operator") ?? stringProp("airline") ?? "—"),
                    ("Speed",     numericProp("speed").map { "\(Int($0)) kt" } ?? "—"),
                    ("Heading",   numericProp("heading").map { "\(Int($0))°" } ?? "—"),
                    ("Altitude",  numericProp("altitude").map { "\(Int($0)) ft" } ?? "—"),
                    ("MMSI",      stringProp("mmsi") ?? "—"),
                    ("ICAO",      stringProp("icao24") ?? "—"),
                    ("Source",    feature.layerId),
                ])

                if let coord = feature.geometry.anchor {
                    PopupSectionHeader("Coordinates", icon: "mappin.and.ellipse")
                    if showsMiniMap {
                        CoordinateMiniMap(coordinate: coord)
                    }
                    CoordinateAddressView(coordinate: coord)
                }
            }
            .padding()
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Vehicle")
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

    private var callsign: String {
        stringProp("callsign") ?? stringProp("name") ?? feature.id
    }

    private func stringProp(_ k: String) -> String? {
        feature.properties[k]?.value as? String
    }
    private func numericProp(_ k: String) -> Double? {
        if let v = feature.properties[k]?.value as? Double { return v }
        if let v = feature.properties[k]?.value as? Int    { return Double(v) }
        return nil
    }

    private func grid(_ rows: [(String, String)]) -> some View {
        VStack(spacing: 1) {
            ForEach(rows, id: \.0) { (k, v) in
                HStack {
                    Text(k).font(.caption.bold()).foregroundStyle(theme.textMuted)
                    Spacer()
                    JapaneseAware(
                        text: v,
                        font: .caption,
                        foregroundStyle: AnyShapeStyle(theme.text),
                        alignment: .trailing
                    )
                }
                .padding(8)
                .background(theme.surfaceElevated)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}
