import SwiftUI
import MapKit
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// Saved areas of interest (roadmap 9) — list, inspect, delete.
//
// Mount inside a NavigationStack (like `AlertsTab`), or present as a sheet from
// one. Optionally pass `onAlertHere` to enable the "Draw new area" button: the
// closure is handed the finished geometry so the caller can open a prefilled
// `AlertEditor`. Without it the button is hidden rather than being a control
// that does nothing.
//
// Delete is the interesting case. The server returns **409 `aoi_in_use`** with
// the referencing rules when a rule still geofences on the shape, because
// dropping it would make those rules fail closed — i.e. stop firing, silently.
// That is a legitimate refusal with a specific remedy, so it is rendered as
// "still used by a rule (…)", never as a raw HTTP error.
// ─────────────────────────────────────────────────────────────────────────────

struct AOIListView: View {
    /// Supply to enable in-place drawing. Receives the finished shape so the
    /// caller can open a prefilled `AlertEditor`.
    var onAlertHere: ((DrawnAOI) -> Void)? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var areas: [AreaOfInterest] = []
    @State private var loading = false
    @State private var error: String?
    /// A 409 refusal, phrased for a human. Separate from `error` so it doesn't
    /// read as a failure of the app.
    @State private var inUseMessage: String?
    @State private var showDraw = false

    var body: some View {
        Group {
            if loading && areas.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if areas.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Areas of interest")
        .toolbar {
            if onAlertHere != nil {
                ToolbarItem(placement: .compatPrimary) {
                    Button { showDraw = true } label: {
                        Image(systemName: "plus.circle.fill")
                    }
                    .accessibilityLabel("Draw new area")
                }
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
                .accessibilityLabel("Refresh areas")
            }
        }
        .task { if areas.isEmpty { await reload() } }
        .sheet(isPresented: $showDraw) {
            AOIDrawOverlay(
                onAlertHere: { drawn in
                    showDraw = false
                    onAlertHere?(drawn)
                },
                onSaved: { saved in
                    areas.insert(saved, at: 0)
                }
            )
        }
        .alert("Area still in use",
               isPresented: Binding(get: { inUseMessage != nil },
                                    set: { if !$0 { inUseMessage = nil } })) {
            Button("OK", role: .cancel) { inUseMessage = nil }
        } message: {
            Text(inUseMessage ?? "")
        }
        .alert("Areas error",
               isPresented: Binding(get: { error != nil && !areas.isEmpty },
                                    set: { if !$0 { error = nil } })) {
            Button("OK", role: .cancel) { error = nil }
        } message: {
            Text(error ?? "")
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            Section {
                ForEach(areas) { aoi in
                    row(aoi)
                }
            } footer: {
                Text("An area a rule still points at can't be deleted — the rule would stop firing and nothing would say so.")
                    .font(.caption2)
            }
        }
        .compatInsetGroupedListStyle()
        .scrollContentBackground(.hidden)
        .refreshable { await reload() }
    }

    private func row(_ aoi: AreaOfInterest) -> some View {
        HStack(alignment: .top, spacing: Space.md) {
            AOIShapePreview(aoi: aoi)
            VStack(alignment: .leading, spacing: 3) {
                Text(aoi.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                HStack(spacing: Space.xs) {
                    Pill(text: aoi.kind.uppercased(),
                         tone: .accent,
                         icon: kindIcon(aoi.kind),
                         maxWidth: 110)
                    Text(geometryDetail(aoi))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(1)
                    Spacer(minLength: 0)
                }
                Text("Added \(relativeTime(aoi.created_at))")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, 2)
        .listRowBackground(theme.surface)
        .swipeActions(edge: .trailing) {
            Button(role: .destructive) {
                Task { await delete(aoi) }
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
        .contextMenu {
            Button {
                Clipboard.copy(aoi.id)
            } label: {
                Label("Copy area id", systemImage: "number")
            }
            Button(role: .destructive) {
                Task { await delete(aoi) }
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No saved areas", systemImage: "mappin.and.ellipse")
                .foregroundStyle(theme.text)
        } description: {
            Text(onAlertHere == nil
                 ? "Draw a polygon or circle on the map and choose “Save as area”. Saved areas can then be reused by any alert rule."
                 : "Draw a polygon or circle, then save it. Saved areas can be reused by any alert rule.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            if onAlertHere != nil {
                Button("Draw an area") { showDraw = true }
                    .buttonStyle(.borderedProminent)
            }
            if let error {
                Text(error)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
            }
        }
    }

    // MARK: - Actions

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            areas = try await apiClient.api.aois()
            error = nil
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func delete(_ aoi: AreaOfInterest) async {
        // Optimistic removal, restored on any refusal. The 409 path is a
        // *correct* answer from the server, so it gets its own wording.
        let snapshot = areas
        areas.removeAll { $0.id == aoi.id }
        do {
            try await apiClient.api.aoiDelete(aoi.id)
            Haptics.warning()
        } catch APIError.http(409, let body) {
            areas = snapshot
            inUseMessage = inUseSentence(name: aoi.name, body: body)
            Haptics.warning()
        } catch let e {
            areas = snapshot
            error = e.localizedDescription
            Haptics.error()
        }
    }

    /// The 409 body is `{"error":"aoi_in_use","rules":[{"id","name"}]}`. Naming
    /// the rules is the whole point — "in use" without saying by what leaves the
    /// user with nothing to act on.
    private func inUseSentence(name: String, body: String) -> String {
        let names = ruleNames(from: body)
        let base = "“\(name)” is still used by "
        if names.isEmpty {
            return base + "an alert rule. Repoint or delete that rule first — deleting the area would leave it unable to fire, without any warning."
        }
        let list = names.joined(separator: ", ")
        let plural = names.count == 1 ? "rule" : "rules"
        return base + "\(names.count) alert \(plural): \(list). Repoint or delete \(names.count == 1 ? "it" : "them") first — deleting the area would leave \(names.count == 1 ? "it" : "them") unable to fire, without any warning."
    }

    private func ruleNames(from body: String) -> [String] {
        guard let data = body.data(using: .utf8),
              let root = try? JSONSerialization.jsonObject(with: data),
              let obj = root as? [String: Any],
              let rules = obj["rules"] as? [[String: Any]] else { return [] }
        return rules.compactMap { $0["name"] as? String }
                    .filter { !$0.trimmingCharacters(in: .whitespaces).isEmpty }
    }

    // MARK: - Display helpers

    private func kindIcon(_ kind: String) -> String {
        switch kind {
        case "polygon": return "pentagon"
        case "circle":  return "circle.dashed"
        default:        return "rectangle.dashed"
        }
    }

    /// Every coordinate here is LABELLED with its axis. Unlabelled, the circle
    /// row read "lat, lon" while the bbox row underneath read `[w, s, e, n]`
    /// — i.e. lon, lat — two opposite axis orders in adjacent rows, which is
    /// exactly how a reader mis-reads a location and doesn't notice.
    private func geometryDetail(_ aoi: AreaOfInterest) -> String {
        if let ring = aoi.polygonRing, !ring.isEmpty {
            return "\(ring.count) vertices"
        }
        if let c = aoi.circle {
            return "r \(DrawnAOI.formatRadius(c.radius)) · lat \(fmt(c.center.latitude)), lon \(fmt(c.center.longitude))"
        }
        if let box = AOIShapePreview.bbox(from: aoi) {
            return "S \(fmt(box[1])), W \(fmt(box[0])) → N \(fmt(box[3])), E \(fmt(box[2]))"
        }
        return "geometry unavailable"
    }

    private func fmt(_ d: Double) -> String { String(format: "%.3f", d) }
}

// MARK: - Thumbnail

/// A cheap, deterministic thumbnail of the stored geometry. Deliberately a
/// `Path` rather than a live `Map`: one MapKit view per row would be an
/// expensive way to draw a 44pt outline, and this can't fail to load.
struct AOIShapePreview: View {
    let aoi: AreaOfInterest

    @Environment(\.theme) private var theme

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: Radius.sm, style: .continuous)
                .fill(theme.surfaceElevated)
            content
        }
        .frame(width: 46, height: 46)
        .accessibilityHidden(true)
    }

    @ViewBuilder
    private var content: some View {
        if let ring = aoi.polygonRing, ring.count >= 3 {
            GeometryReader { geo in
                let rect = CGRect(origin: .zero, size: geo.size).insetBy(dx: 5, dy: 5)
                let path = Self.path(for: ring, in: rect)
                ZStack {
                    path.fill(theme.accent.opacity(0.25))
                    path.stroke(theme.accent, lineWidth: 1.3)
                }
            }
        } else if aoi.circle != nil {
            ZStack {
                Circle().fill(theme.accent.opacity(0.25))
                Circle().strokeBorder(theme.accent, lineWidth: 1.3)
            }
            .padding(6)
        } else if let box = Self.bbox(from: aoi) {
            // Draw the box with its real aspect ratio so a long corridor and a
            // square don't look identical.
            GeometryReader { geo in
                let rect = CGRect(origin: .zero, size: geo.size).insetBy(dx: 5, dy: 5)
                let ring = [
                    CLLocationCoordinate2D(latitude: box[1], longitude: box[0]),
                    CLLocationCoordinate2D(latitude: box[3], longitude: box[0]),
                    CLLocationCoordinate2D(latitude: box[3], longitude: box[2]),
                    CLLocationCoordinate2D(latitude: box[1], longitude: box[2])
                ]
                let path = Self.path(for: ring, in: rect)
                ZStack {
                    path.fill(theme.accent.opacity(0.25))
                    path.stroke(theme.accent, lineWidth: 1.3)
                }
            }
        } else {
            Image(systemName: "questionmark")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
    }

    /// `[w, s, e, n]`, taken from the server's own `bbox` field rather than
    /// re-derived from the geometry array. Callers reach this only after the
    /// polygon and circle branches have been ruled out, so the box a polygon
    /// also carries is never what gets drawn.
    static func bbox(from aoi: AreaOfInterest) -> [Double]? {
        guard let bb = aoi.bbox, bb.count >= 4 else { return nil }
        return Array(bb.prefix(4))
    }

    /// Uniform-scale the ring into `rect`, flipping latitude so north is up.
    static func path(for ring: [CLLocationCoordinate2D], in rect: CGRect) -> Path {
        var p = Path()
        guard ring.count >= 2, rect.width > 0, rect.height > 0 else { return p }
        let lons = ring.map(\.longitude)
        let lats = ring.map(\.latitude)
        guard let minLon = lons.min(), let maxLon = lons.max(),
              let minLat = lats.min(), let maxLat = lats.max() else { return p }
        let w = max(maxLon - minLon, 1e-9)
        let h = max(maxLat - minLat, 1e-9)
        let scale = min(rect.width / w, rect.height / h)
        let originX = rect.midX - (w * scale) / 2
        let originY = rect.midY - (h * scale) / 2

        func point(_ c: CLLocationCoordinate2D) -> CGPoint {
            CGPoint(x: originX + (c.longitude - minLon) * scale,
                    y: originY + (maxLat - c.latitude) * scale)
        }

        p.move(to: point(ring[0]))
        for c in ring.dropFirst() { p.addLine(to: point(c)) }
        p.closeSubpath()
        return p
    }
}
