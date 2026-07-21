import SwiftUI
import CoreLocation

/// Level-3 detail: full body + properties table for a single intel item.
/// Lazy-loads via `/api/intel/items/:uid` so the row stays light.
struct IntelDetail: View {
    let uid: String
    let fallbackTitle: String

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme

    @State private var item: IntelItem?
    @State private var loading = true
    @State private var error: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                header
                if loading {
                    ProgressView()
                } else if let error {
                    Text(error).font(.caption).foregroundStyle(theme.warning)
                } else if let item {
                    metaRow(item)
                    if let body = item.body, !body.isEmpty {
                        JapaneseAware(
                            text: body,
                            font: .body,
                            foregroundStyle: AnyShapeStyle(theme.text)
                        )
                    } else if let summary = item.summary, !summary.isEmpty {
                        JapaneseAware(
                            text: summary,
                            font: .body,
                            foregroundStyle: AnyShapeStyle(theme.text)
                        )
                    }
                    if let urlStr = item.link, let url = URL(string: urlStr) {
                        Link(destination: url) {
                            Label("Open source", systemImage: "safari")
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    if let prov = item.provenance {
                        Text("Provenance").font(.headline).foregroundStyle(theme.text)
                        provenanceCard(prov)
                    }
                    if let props = item.properties, !props.isEmpty {
                        Text("Properties").font(.headline).foregroundStyle(theme.text)
                        propertiesGrid(props)
                    }
                    if let coord = coordinate(from: item) {
                        Text("Location").font(.headline).foregroundStyle(theme.text)
                        CoordinateMiniMap(coordinate: coord)
                        CoordinateAddressView(coordinate: coord)
                        Button {
                            nav.showOnMap(coord, feature: mapFeature(from: item, at: coord))
                        } label: {
                            Label("Show on map", systemImage: "map")
                        }
                        .buttonStyle(.bordered)
                    }
                }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle(item?.title ?? fallbackTitle)
        .compatInlineTitle()
        .task { await load() }
    }

    private var header: some View {
        TranslatableHeader(text: item?.title ?? fallbackTitle) { EmptyView() }
    }

    private func metaRow(_ it: IntelItem) -> some View {
        HStack(spacing: 8) {
            Text(relativeTime(it.published_at ?? it.fetched_at))
                .font(.caption.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            Text("·").font(.caption).foregroundStyle(theme.textMuted)
            Text(it.source_id)
                .font(.caption.monospaced())
                .foregroundStyle(theme.textMuted)
            if let author = it.author {
                Text("·").font(.caption).foregroundStyle(theme.textMuted)
                Text(author)
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
            }
            Spacer(minLength: 0)
        }
    }

    /// The "full info path" for this data point: which collector emitted it,
    /// the upstream origin the collector pulls from, this datum's own link,
    /// when it was fetched/published, and license/confidence when known.
    @ViewBuilder
    private func provenanceCard(_ p: Provenance) -> some View {
        let rows: [(String, String?)] = [
            ("Collector", provCollector(p)),
            ("Method", p.collection_method),
            ("Category", p.category),
            ("Source origin", p.source_url),
            ("Item link", p.item_url),
            ("Channel", p.sub_source_id),
            ("Fetched", relativeTime(p.fetched_at)),
            ("Published", p.published_at.map { relativeTime($0) }),
            ("License", p.license),
            ("Confidence", p.confidence.map { String(format: "%.0f%%", $0 * 100) }),
        ]
        VStack(spacing: 1) {
            ForEach(rows.indices, id: \.self) { i in
                let (label, value) = rows[i]
                if let value, !value.isEmpty, value != "—" {
                    HStack(alignment: .top, spacing: 8) {
                        Text(label)
                            .font(.caption.bold())
                            .foregroundStyle(theme.textMuted)
                            .frame(width: 110, alignment: .leading)
                        if (label == "Source origin" || label == "Item link"),
                           let url = URL(string: value) {
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
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    private func provCollector(_ p: Provenance) -> String {
        let id = p.source_id ?? "?"
        if let name = p.source_name, name != id { return "\(name)  ·  \(id)" }
        return id
    }

    private func propertiesGrid(_ properties: [String: AnyCodable]) -> some View {
        VStack(spacing: 1) {
            ForEach(properties.keys.sorted(), id: \.self) { key in
                HStack(alignment: .top, spacing: 8) {
                    Text(key)
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 110, alignment: .leading)
                    JapaneseAware(
                        text: stringify(properties[key]?.value),
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

    /// Pull a map-able coordinate for this datum. The geocoded point is served
    /// top-level (`item.lat`/`item.lon`, from the `intel_items` columns); most
    /// geolocated items — e.g. OSM points whose lat/lon live in `geometry`, not
    /// `properties` — only have it there. We fall back to lat/lon embedded in
    /// the free-form `properties` (collectors emit them under interchangeable
    /// keys and as Int, Double, or String). Either way we reject out-of-range or
    /// null-island (0,0) values so a missing coordinate never renders a bogus
    /// pin in the ocean.
    private func coordinate(from item: IntelItem) -> CLLocationCoordinate2D? {
        var lat = item.lat
        var lon = item.lon
        if lat == nil || lon == nil, let props = item.properties {
            lat = lat ?? firstDouble(props, ["latitude", "lat"])
            lon = lon ?? firstDouble(props, ["longitude", "lon", "lng"])
        }
        guard let lat, let lon else { return nil }
        guard (-90...90).contains(lat), (-180...180).contains(lon) else { return nil }
        guard lat != 0 || lon != 0 else { return nil }
        return CLLocationCoordinate2D(latitude: lat, longitude: lon)
    }

    /// Synthesize a `GeoFeature` so "Show on map" can fly the main Map to this
    /// datum and present its popup — even when no intel layer is toggled on
    /// (MapNavigation presents `pendingPresent` regardless). The generic
    /// `intel` layerId routes to the default `FeaturePopup`. We pass the item's
    /// own `properties` through and surface title/link/source so the popup's
    /// name, external link, and provenance render.
    private func mapFeature(from item: IntelItem, at coord: CLLocationCoordinate2D) -> GeoFeature {
        var props = item.properties ?? [:]
        if props["title"] == nil, let t = item.title { props["title"] = AnyCodable(t) }
        if props["link"] == nil, let l = item.link { props["link"] = AnyCodable(l) }
        if props["source"] == nil { props["source"] = AnyCodable(item.source_id) }
        return GeoFeature(id: "intel|\(item.uid)", layerId: "intel",
                          geometry: .point(coord), properties: props)
    }

    private func firstDouble(_ props: [String: AnyCodable], _ keys: [String]) -> Double? {
        for key in keys {
            switch props[key]?.value {
            case let v as Double: return v
            case let v as Int:    return Double(v)
            case let v as String: if let d = Double(v) { return d }
            default:              continue
            }
        }
        return nil
    }

    private func stringify(_ value: Any?) -> String {
        switch value {
        case nil:                 return "—"
        case let v as String:     return v
        case let v as NSNumber:   return v.stringValue
        case let v as Bool:       return v ? "true" : "false"
        case let v as [Any]:      return v.map { String(describing: $0) }.joined(separator: ", ")
        case let v as [String: Any]: return "{\(v.count) keys}"
        default:                  return String(describing: value!)
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        do {
            item = try await apiClient.api.intelItem(uid: uid)
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }
}
