import SwiftUI

/// Entity search list — FTS over the unified graph (collector NER +
/// OSINT-search discoveries; kanji-substring works via kuromoji).
struct EntitiesView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var q = ""
    @State private var hits: [EntityHit] = []

    var body: some View {
        NavigationStack {
            List {
                ForEach(hits) { h in
                    NavigationLink {
                        EntityDetailView(type: h.type.lowercased(), entityId: h.entity_id)
                    } label: {
                        HStack {
                            Text(h.type).font(.caption2).foregroundColor(theme.accentAlt)
                            Text(h.value).font(.subheadline).lineLimit(1)
                            Spacer()
                            Text("\(h.mention_count ?? 0)").font(.caption2).foregroundColor(theme.textMuted)
                        }
                    }
                }
            }
            .searchable(text: $q)
            .navigationTitle("Entities")
            .background(theme.surface.ignoresSafeArea())
        }
        .onChange(of: q) { _, v in
            Task {
                guard v.trimmingCharacters(in: .whitespaces).count >= 2 else { hits = []; return }
                try? await Task.sleep(nanoseconds: 250_000_000)
                if Task.isCancelled { return }
                hits = (try? await apiClient.api.entitySearch(v)) ?? []
            }
        }
    }
}

struct EntityDetailView: View {
    let type: String
    var entityId: String? = nil
    var lookup: String? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var resolvedId: String?
    @State private var profile: EntityProfile?
    @State private var graph = EntityGraph(nodes: [], edges: [])
    @State private var mentions: [EntityMention] = []
    @State private var tab = 0
    @State private var depth = 1
    @State private var notFound = false

    var body: some View {
        Group {
            if notFound {
                Text("Entity not found — it may not have been extracted yet.")
                    .foregroundColor(theme.textMuted).padding()
            } else if let p = profile {
                content(p)
            } else {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(profile?.value ?? "Entity")
        .task { await load() }
    }

    private func content(_ p: EntityProfile) -> some View {
        VStack(spacing: 0) {
            Picker("", selection: $tab) {
                Text("Relationships").tag(0); Text("Timeline").tag(1); Text("Info").tag(2)
            }.pickerStyle(.segmented).padding(8)

            if tab == 0 {
                VStack {
                    Picker("Depth", selection: $depth) {
                        ForEach(1...3, id: \.self) { Text("\($0)").tag($0) }
                    }.pickerStyle(.segmented).padding(.horizontal, 8)
                    EntityGraphCanvas(graph: graph, rootId: p.entity_id)
                }
                .onChange(of: depth) { _, _ in Task { await loadGraph(p) } }
            } else if tab == 1 {
                List(mentions) { m in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(m.title ?? m.surface ?? "(untitled)").font(.subheadline)
                        Text("\(m.source_id ?? "") · \(m.field ?? "") · \(m.published_at ?? m.created_at ?? "")")
                            .font(.caption2).foregroundColor(theme.textMuted)
                    }
                }
            } else {
                List {
                    LabeledContent("Type", value: p.type)
                    LabeledContent("Value", value: p.value)
                    if let a = p.aliases, !a.isEmpty { LabeledContent("Aliases", value: a.joined(separator: ", ")) }
                    LabeledContent("Mentions", value: "\(p.mention_count ?? 0)")
                    if let f = p.first_seen_at { LabeledContent("First seen", value: f) }
                }
            }
        }
    }

    private func load() async {
        var id = entityId
        if id == nil, let q = lookup {
            let hits = (try? await apiClient.api.entitySearch(q, type: type, limit: 1)) ?? []
            id = hits.first?.entity_id
        }
        guard let eid = id else { notFound = true; return }
        resolvedId = eid
        profile = try? await apiClient.api.entity(type, eid)
        if profile == nil { notFound = true; return }
        await loadGraph(profile!)
        mentions = (try? await apiClient.api.entityMentions(type, eid)) ?? []
    }
    private func loadGraph(_ p: EntityProfile) async {
        graph = (try? await apiClient.api.entityGraph(type, p.entity_id, depth: depth))
            ?? EntityGraph(nodes: [], edges: [])
    }
}

/// Native force-directed graph (SwiftUI Canvas; no WebView). Fixed-iteration
/// layout computed once; tap a node to pivot.
struct EntityGraphCanvas: View {
    let graph: EntityGraph
    let rootId: String
    @Environment(\.theme) private var theme
    @State private var pos: [String: CGPoint] = [:]

    var body: some View {
        GeometryReader { geo in
            Canvas { ctx, size in
                for e in graph.edges {
                    guard let a = pos[e.source], let b = pos[e.target] else { continue }
                    var path = Path(); path.move(to: a); path.addLine(to: b)
                    ctx.stroke(path, with: .color(theme.textMuted.opacity(0.4)), lineWidth: 1)
                }
                for n in graph.nodes {
                    guard let p = pos[n.id] else { continue }
                    let isRoot = n.id == rootId
                    let r: CGFloat = isRoot ? 9 : 6
                    ctx.fill(Path(ellipseIn: CGRect(x: p.x - r, y: p.y - r, width: 2*r, height: 2*r)),
                             with: .color(isRoot ? theme.accentAlt : theme.accent))
                    ctx.draw(Text(String((n.label ?? n.value).prefix(18))).font(.system(size: 9))
                                .foregroundColor(theme.text), at: CGPoint(x: p.x, y: p.y - 14))
                }
            }
            .onAppear { layout(in: geo.size) }
            .onChange(of: graph) { _, _ in layout(in: geo.size) }
        }
        .frame(height: 360)
        .background(RoundedRectangle(cornerRadius: 8).fill(theme.surfaceElevated))
    }

    private func layout(in size: CGSize) {
        guard !graph.nodes.isEmpty, graph.nodes.count <= 120 else { return }
        let W = size.width, H = size.height
        var P: [String: CGPoint] = [:]
        var V: [String: CGVector] = [:]
        for (i, n) in graph.nodes.enumerated() {
            if n.id == rootId { P[n.id] = CGPoint(x: W/2, y: H/2) }
            else {
                let a = Double(i) / Double(graph.nodes.count) * 2 * .pi
                P[n.id] = CGPoint(x: W/2 + cos(a)*130, y: H/2 + sin(a)*120)
            }
            V[n.id] = .zero
        }
        for _ in 0..<140 {
            let ids = Array(P.keys)
            for i in 0..<ids.count {
                for j in (i+1)..<ids.count {
                    let a = P[ids[i]]!, b = P[ids[j]]!
                    var dx = a.x - b.x, dy = a.y - b.y
                    let d2 = max(dx*dx + dy*dy, 0.01), d = sqrt(d2)
                    let f = 3200 / d2; dx /= d; dy /= d
                    V[ids[i]]!.dx += dx*f; V[ids[i]]!.dy += dy*f
                    V[ids[j]]!.dx -= dx*f; V[ids[j]]!.dy -= dy*f
                }
            }
            for e in graph.edges {
                guard let a = P[e.source], let b = P[e.target] else { continue }
                let dx = b.x - a.x, dy = b.y - a.y
                let d = max(sqrt(dx*dx + dy*dy), 0.01)
                let f = (d - 110) * 0.02
                V[e.source]!.dx += dx/d*f; V[e.source]!.dy += dy/d*f
                V[e.target]!.dx -= dx/d*f; V[e.target]!.dy -= dy/d*f
            }
            for id in ids {
                if id == rootId { P[id] = CGPoint(x: W/2, y: H/2); V[id] = .zero; continue }
                V[id]!.dx *= 0.82; V[id]!.dy *= 0.82
                P[id]!.x = min(max(20, P[id]!.x + V[id]!.dx), W - 20)
                P[id]!.y = min(max(20, P[id]!.y + V[id]!.dy), H - 20)
            }
        }
        pos = P
    }
}
