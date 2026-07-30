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
    @State private var entities: [ItemEntity] = []
    /// Roadmap 27 — media assets (EXIF/OCR/pHash) for this item, fetched in
    /// `load()`; the Media section stays hidden when empty.
    @State private var media: [MediaAsset] = []
    @State private var revealed: RevealResult?
    @State private var revealing = false
    @State private var revealError: String?

    /// Breach records carry a "breach:" uid and (when a secret exists) an
    /// operator-revealable credential. Detected off the uid + properties so we
    /// don't need a dedicated record_type field on `IntelItem`.
    private var isBreach: Bool { item?.uid.hasPrefix("breach:") ?? false }
    private var hasSecret: Bool {
        guard let v = item?.properties?["has_secret"]?.value else { return false }
        if let b = v as? Bool { return b }
        if let n = v as? NSNumber { return n.boolValue }
        return false
    }

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
                    // Camera records reach the Intel tab as ordinary intel
                    // items, carrying the same feed properties the map popup
                    // reads. Without this the whole point of the record — the
                    // live picture — was only ever a `url` string in the
                    // properties table below.
                    if let fields = cameraFields(for: item) {
                        Text("Camera feed").font(.headline).foregroundStyle(theme.text)
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
                        if let raw = fields.pageURL, let url = URL(string: raw) {
                            Link(destination: url) {
                                Label("Open camera page", systemImage: "safari")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.borderedProminent)
                        }
                    }
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
                    // Suppressed when the camera section above already renders
                    // this exact URL as "Open camera page" — collectors put the
                    // camera page in both `link` and `properties.url`.
                    if let urlStr = item.link, let url = URL(string: urlStr),
                       urlStr != cameraFields(for: item)?.pageURL {
                        Link(destination: url) {
                            Label("Open source", systemImage: "safari")
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    if !entities.isEmpty { entityChips }
                    if isBreach && hasSecret { revealSection }
                    if let prov = item.provenance {
                        Text("Provenance").font(.headline).foregroundStyle(theme.text)
                        provenanceCard(prov)
                    }
                    if let props = displayProperties(for: item), !props.isEmpty {
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
                            // The label — not the button — carries the width so
                            // the bordered chrome spans the column and the
                            // icon+text sit centred inside it. Widening the
                            // Button instead leaves a hit-target-sized pill
                            // pinned to the leading edge of the VStack.
                            Label("Show on map", systemImage: "map")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                    }

                    // Analyst enrichment surfaces (roadmap 15 / 17 / 27). Each
                    // fetches its own data and renders nothing until it has some.
                    if !media.isEmpty {
                        MediaSection(assets: media)
                    }
                    EvidenceSection(itemUid: uid)
                    AnnotationsSection(refType: "intel_item", refId: uid)
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

    // MARK: - Cameras

    /// Feed inputs when this item is a camera record, else nil. Camera
    /// collectors write to `intel_items` like every other source, so the only
    /// signal is the property bag — see `CameraFeedFields.describesCamera`.
    private func cameraFields(for item: IntelItem) -> CameraFeedFields? {
        guard let props = item.properties, CameraFeedFields.describesCamera(props) else {
            return nil
        }
        return CameraFeedFields(properties: props)
    }

    /// Properties minus the keys the camera section already renders, so the
    /// table doesn't repeat the feed URLs underneath the live picture.
    private func displayProperties(for item: IntelItem) -> [String: AnyCodable]? {
        guard let props = item.properties, !props.isEmpty else { return nil }
        guard cameraFields(for: item) != nil else { return props }
        let consumed = CameraFeedFields.consumedKeys.union(["name", "name_ja", "title"])
        return props.filter { !consumed.contains($0.key) }
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
            // Non-fatal: an item with no extracted entities simply shows no chips.
            entities = (try? await apiClient.api.itemEntities(uid: uid)) ?? []
            // Roadmap 27 — EXIF/OCR/pHash media for this item, if any. Best
            // effort: no media just means the section stays hidden.
            media = (try? await apiClient.api.mediaAssets(forItem: uid)) ?? []
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }

    // MARK: - Entity chips (breach + normal items)

    @ViewBuilder
    private var entityChips: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Entities").font(.headline).foregroundStyle(theme.text)
            FlowLayout(spacing: 6) {
                ForEach(entities) { e in
                    NavigationLink {
                        EntityDetailView(type: e.type.lowercased(), entityId: e.entity_id)
                    } label: {
                        HStack(spacing: 4) {
                            Text(e.type)
                                .font(.caption2.weight(.semibold))
                                .foregroundStyle(theme.textMuted)
                            Text(e.label ?? e.value)
                                .font(.caption2)
                                .foregroundStyle(theme.text)
                                .lineLimit(1)
                        }
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(Capsule().fill(theme.accentAlt.opacity(0.15)))
                        .overlay(Capsule().strokeBorder(theme.accentAlt.opacity(0.3), lineWidth: 0.5))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    // MARK: - Operator credential reveal (breach records)

    @ViewBuilder
    private var revealSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            if let r = revealed {
                Text("Revealed credential").font(.headline).foregroundStyle(theme.text)
                if let note = r.note {
                    Text(note).font(.caption).foregroundStyle(theme.textMuted)
                }
                ForEach(r.breaches ?? []) { b in
                    HStack {
                        if let br = b.breach {
                            Text(br).font(.caption).foregroundStyle(theme.textMuted)
                        }
                        Spacer()
                        if let s = b.secret {
                            Text(s)
                                .font(.body.monospaced())
                                .textSelection(.enabled)
                                .foregroundStyle(theme.text)
                        }
                    }
                    .padding(8)
                    .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
                }
            } else {
                Button {
                    Task { await doReveal() }
                } label: {
                    if revealing {
                        ProgressView().controlSize(.small)
                    } else {
                        Label("Reveal credential", systemImage: "eye")
                    }
                }
                .buttonStyle(.bordered)
                .disabled(revealing)
                if let e = revealError {
                    Text(e).font(.caption2).foregroundStyle(theme.danger)
                }
            }
        }
    }

    private func doReveal() async {
        revealing = true
        revealError = nil
        defer { revealing = false }
        do {
            revealed = try await apiClient.api.revealItem(uid: uid)
        } catch {
            revealError = "Reveal failed — operator access required."
        }
    }
}
