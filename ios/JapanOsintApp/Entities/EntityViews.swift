import SwiftUI

/// Entity search list — FTS over the unified graph (collector NER +
/// OSINT-search discoveries; kanji-substring works via kuromoji).
struct EntitiesView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var q = ""
    @State private var hits: [EntityHit] = []
    /// The in-flight debounce+fetch. Held so the NEXT keystroke can cancel it —
    /// an unstructured `Task {}` whose handle is discarded is never cancelled,
    /// which turns `Task.isCancelled` into a constant `false` and the sleep into
    /// a fixed delay rather than a debounce, letting a slow earlier request land
    /// on top of newer results.
    @State private var searchTask: Task<Void, Never>?

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
            .themedScreenBackground(theme)
        }
        .onChange(of: q) { _, v in
            searchTask?.cancel()
            let query = v.trimmingCharacters(in: .whitespaces)
            guard query.count >= 2 else { hits = []; return }
            searchTask = Task {
                try? await Task.sleep(nanoseconds: 250_000_000)
                if Task.isCancelled { return }
                let found = (try? await apiClient.api.entitySearch(query)) ?? []
                // Re-check after the await: a keystroke during the request must
                // not have its newer results overwritten by this older one.
                if Task.isCancelled { return }
                hits = found
            }
        }
        .onDisappear { searchTask?.cancel() }
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
    @State private var mentions: [EntityMention] = []
    @State private var tab = 0
    @State private var notFound = false
    @State private var pushedNode: GraphNode?

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
        .themedScreenBackground(theme)
        .navigationTitle(profile?.value ?? "Entity")
        .task { await load() }
    }

    private func content(_ p: EntityProfile) -> some View {
        VStack(spacing: 0) {
            Picker("", selection: $tab) {
                Text("Relationships").tag(0); Text("Timeline").tag(1); Text("Info").tag(2)
            }.pickerStyle(.segmented).padding(8)

            if tab == 0 {
                // `GraphCanvasView` owns its own depth / relationship-type /
                // hub-collapse controls and its own fetch. It replaced the crude
                // canvas that used to live here because it REPORTS what the
                // server truncated instead of rendering a blank frame the moment
                // the graph outgrew a hard-coded node ceiling.
                // Labelled, not a trailing closure: the init's last parameter is
                // `onAddVisibleToCase`, and spelling the label out means the
                // handler can never be matched to the wrong seam.
                GraphCanvasView(entityType: type,
                                entityId: p.entity_id,
                                onOpenEntity: { node in pushedNode = node })
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

                    // Roadmap 23 — breach exposure for this entity; self-fetches
                    // and stays hidden when the entity has none.
                    ExposureSection(entityType: type, entityId: p.entity_id)
                    // Roadmap 15 — analyst notes pinned to this entity.
                    AnnotationsSection(refType: "entity", refId: p.entity_id)
                }
            }
        }
        // Tapping a relationship-graph node pushes that entity's own profile.
        // `node.type` is passed VERBATIM: it came straight out of the server's
        // own `entities.type` column and the graph route matches it with
        // `strcmp`, so lower-casing it here could turn a valid id into a 404.
        .navigationDestination(item: $pushedNode) { node in
            EntityDetailView(type: node.type, entityId: node.id)
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
        // The relationship graph fetches itself — `GraphCanvasView` owns its
        // own depth/filters and its own request lifecycle.
        mentions = (try? await apiClient.api.entityMentions(type, eid)) ?? []
    }
}
