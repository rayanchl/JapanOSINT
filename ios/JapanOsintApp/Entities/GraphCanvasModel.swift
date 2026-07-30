import SwiftUI
import Combine

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 19 — entity graph canvas: layout + fetch/merge state.
//
// Two jobs:
//
// 1. A bounded force-directed layout. Repulsion between every pair (with a
//    distance cutoff so a spread-out graph stops paying for far-apart pairs),
//    springs along edges, velocity damping. It runs a few iterations per
//    animation tick and then STOPS — `settled` flips true, which the view feeds
//    to `TimelineView(.animation(paused:))` so the render loop actually halts
//    instead of burning a core forever on a graph that has already converged.
//
// 2. Fetch + merge, and — the part that matters most — the accounting of what
//    is NOT on screen. `GraphMeta.truncated` is never swallowed. The server
//    drops nodes for two different reasons (a hub was too connected to expand,
//    or the node cap was reached) and this canvas can drop a third way (a merge
//    that would push past `nodeCap`). All three are counted and surfaced by
//    `truncationSummary`. A canvas that silently drops half a graph teaches an
//    analyst to trust a picture that is lying to them.
// ─────────────────────────────────────────────────────────────────────────────

/// One drawable edge, pre-resolved to node indices so the render pass never
/// does a dictionary lookup per edge.
struct GraphLink: Identifiable, Hashable {
    let edge: GraphEdge
    let a: Int
    let b: Int
    var id: String { edge.id }
}

@MainActor
final class GraphCanvasModel: ObservableObject {

    // ── Identity ───────────────────────────────────────────────────────────

    let rootType: String
    let rootId: String

    /// Server-side and canvas-side node ceiling. Kept equal to the server's own
    /// default (`entityapi_graph` caps at 300) so "the cap" means one number.
    static let nodeCap = 300
    /// Degree above which a node counts as a hub. Sent as `exclude_hubs` — a
    /// hub is still SHOWN (the edge that found it is real intel), it is just
    /// never expanded through.
    static let hubDegreeThreshold = 50

    static let relTypeOptions = ["asserted", "co_mention", "pivot_discovered"]

    // ── Controls ───────────────────────────────────────────────────────────

    @Published var depth: Int = 1
    /// Empty means "no filter" — which is exactly what the server does with an
    /// absent `rel_types`, so the two agree without a special case.
    @Published var relTypes: Set<String> = []
    @Published var excludeHubs: Bool = false

    // ── Graph data ─────────────────────────────────────────────────────────

    @Published private(set) var nodes: [GraphNode] = []
    @Published private(set) var edges: [GraphEdge] = []
    @Published private(set) var links: [GraphLink] = []
    /// Meta from the last root fetch.
    @Published private(set) var rootMeta: GraphMeta?
    /// Per-expansion truncation, keyed by the node that was expanded. Merging
    /// an ego-network can hide things too, and that has to be counted.
    @Published private(set) var expansionDrops: [String: Int] = [:]
    /// Nodes this canvas itself refused because the merge would have exceeded
    /// `nodeCap`.
    @Published private(set) var mergeDropped: Int = 0
    @Published private(set) var expanded: Set<String> = []

    // ── Layout output (published; read by the Canvas) ───────────────────────

    /// Positions, parallel to `nodes`. Written once per tick, not per node, so
    /// one animation frame is one publish.
    @Published private(set) var points: [CGPoint] = []
    @Published private(set) var pinned: Set<String> = []
    @Published var selected: String?
    /// True once the integrator has converged or hit its iteration ceiling.
    /// Drives `TimelineView(.animation(paused:))` in the view.
    @Published private(set) var settled: Bool = true

    // ── Status ─────────────────────────────────────────────────────────────

    @Published private(set) var loading = false
    @Published private(set) var expanding: String?
    @Published private(set) var error: String?

    // ── Private layout storage ─────────────────────────────────────────────

    private var px: [CGFloat] = []
    private var py: [CGFloat] = []
    private var vx: [CGFloat] = []
    private var vy: [CGFloat] = []
    /// Ids in the same order as `px`/`py`, so positions survive a merge.
    private var laidOutIDs: [String] = []
    private var canvas = CGSize(width: 320, height: 320)
    private var iteration = 0
    private let maxIterations = 320
    /// Monotonic request id; a response whose token is stale is discarded.
    private var loadToken = 0

    private var maxMentions = 1
    private var maxWeight: Double = 1

    // Physics constants. Same family of numbers as the small graph already in
    // `EntityViews.swift`, retuned for up to 300 nodes.
    private let repulsion: CGFloat = 3400
    private let repulsionCutoff: CGFloat = 260
    private let springStiffness: CGFloat = 0.020
    private let springRest: CGFloat = 105
    private let damping: CGFloat = 0.84
    private let settleThreshold: CGFloat = 0.35

    init(rootType: String, rootId: String) {
        self.rootType = rootType
        self.rootId = rootId
    }

    // ── Derived: truncation accounting ─────────────────────────────────────

    /// Nodes the user is NOT seeing, as best as it can be counted. The server's
    /// `hubs_collapsed` counts expansions it declined rather than distinct
    /// entities, so this is a floor, never an exact figure — which is why the
    /// banner says "at least".
    var hiddenCount: Int {
        var hidden = 0
        if let m = rootMeta { hidden += m.hubs_collapsed + m.nodes_over_cap }
        hidden += expansionDrops.values.reduce(0, +)
        hidden += mergeDropped
        return hidden
    }

    var isTruncated: Bool {
        hiddenCount > 0 || (rootMeta?.truncated ?? false)
    }

    /// Headline banner text. nil only when nothing at all was dropped.
    var truncationSummary: String? {
        guard isTruncated else { return nil }
        guard hiddenCount > 0 else {
            // The server flagged truncation without attributing a count. Say so
            // rather than printing "showing 40 of at least 40", which would read
            // as reassurance.
            return "Showing \(nodes.count) nodes — the server flagged this graph as incomplete."
        }
        return "Showing \(nodes.count) of at least \(nodes.count + hiddenCount) nodes."
    }

    /// The reasons, itemised, for the banner's second line.
    var truncationReasons: [String] {
        var parts: [String] = []
        if let m = rootMeta {
            if m.hubs_collapsed > 0 {
                parts.append("\(m.hubs_collapsed) hub\(m.hubs_collapsed == 1 ? "" : "s") collapsed "
                             + "(too connected to expand through)")
            }
            if m.nodes_over_cap > 0 {
                parts.append("\(m.nodes_over_cap) past the server's \(m.max_nodes)-node cap")
            }
        }
        let expansionTotal = expansionDrops.values.reduce(0, +)
        if expansionTotal > 0 {
            parts.append("\(expansionTotal) dropped by the server while expanding a node")
        }
        if mergeDropped > 0 {
            parts.append("\(mergeDropped) refused by this canvas at its \(Self.nodeCap)-node cap")
        }
        if parts.isEmpty && (rootMeta?.truncated ?? false) {
            parts.append("the server reported this graph as truncated")
        }
        return parts
    }

    /// Distinct entity types present, for the colour legend.
    var typesPresent: [String] {
        var seen: [String] = []
        for n in nodes where !seen.contains(n.type) { seen.append(n.type) }
        return seen.sorted()
    }

    var selectedNode: GraphNode? {
        guard let id = selected else { return nil }
        return nodes.first { $0.id == id }
    }

    /// Edges touching the selected node, for the detail chip.
    func edgeCount(for id: String) -> Int {
        edges.reduce(0) { $0 + (($1.source == id || $1.target == id) ? 1 : 0) }
    }

    // ── Fetch ──────────────────────────────────────────────────────────────

    private var relTypesParam: [String]? {
        relTypes.isEmpty ? nil : relTypes.sorted()
    }

    private var excludeHubsParam: Int? {
        excludeHubs ? Self.hubDegreeThreshold : nil
    }

    /// (Re)load the root ego-network. Resets every derived piece of state,
    /// including the truncation accounting — stale drop counts from a previous
    /// depth would misdescribe the picture now on screen.
    func load(using api: API) async {
        // Two quick taps on the depth control can leave two loads in flight;
        // the older one must not be allowed to land on top of the newer.
        loadToken &+= 1
        let token = loadToken
        loading = true
        do {
            let graph = try await api.entityEgoGraph(
                type: rootType, id: rootId, depth: depth,
                relTypes: relTypesParam,
                excludeHubs: excludeHubsParam,
                maxNodes: Self.nodeCap
            )
            guard token == loadToken else { return }
            nodes = graph.nodes
            edges = graph.edges
            rootMeta = graph.meta
            expanded = []
            expansionDrops = [:]
            mergeDropped = 0
            pinned = []
            selected = nil
            rebuild(preservePositions: false)
            error = nil
        } catch {
            guard token == loadToken else { return }
            self.error = GraphCanvasModel.explain(error)
            Haptics.error()
        }
        guard token == loadToken else { return }
        loading = false
    }

    /// Double-tap: pull one node's own ego-network and fold it into the canvas.
    /// Depth is deliberately 1 — the user asked about THIS node, and a depth-3
    /// expansion from an arbitrary node is how a readable graph becomes a hairball.
    func expand(_ nodeId: String, using api: API) async {
        guard expanding == nil, !loading else { return }
        guard let node = nodes.first(where: { $0.id == nodeId }) else { return }
        // If a root reload lands while this ego fetch is in flight, the graph
        // it was meant to grow no longer exists — merging into the new one
        // would silently mix two different questions.
        let token = loadToken
        expanding = nodeId
        do {
            let ego = try await api.entityEgoGraph(
                // `type` is passed through verbatim: the server validates it
                // against the stored entities.type, so normalising the case
                // here would turn a valid id into a 404.
                type: node.type, id: node.id, depth: 1,
                relTypes: relTypesParam,
                excludeHubs: excludeHubsParam,
                maxNodes: Self.nodeCap
            )
            guard token == loadToken else { expanding = nil; return }
            merge(nodes: ego.nodes, edges: ego.edges, meta: ego.meta, origin: nodeId)
            expanded.insert(nodeId)
            error = nil
            Haptics.success()
        } catch {
            guard token == loadToken else { expanding = nil; return }
            self.error = GraphCanvasModel.explain(error)
            Haptics.error()
        }
        expanding = nil
    }

    /// Takes the three pieces rather than the envelope type on purpose: the
    /// graph envelope's name is declared elsewhere and this file has no need to
    /// spell it, so a rename over there can't reach in here.
    private func merge(nodes incoming: [GraphNode], edges incomingEdges: [GraphEdge],
                       meta: GraphMeta?, origin: String) {
        var known = Set(nodes.map { $0.id })
        var appendedNodes: [GraphNode] = []
        var dropped = 0
        for candidate in incoming where !known.contains(candidate.id) {
            if nodes.count + appendedNodes.count >= Self.nodeCap {
                dropped += 1
                continue
            }
            appendedNodes.append(candidate)
            known.insert(candidate.id)
        }

        var edgeKeys = Set(edges.map { $0.id })
        var appendedEdges: [GraphEdge] = []
        for candidate in incomingEdges where !edgeKeys.contains(candidate.id) {
            // An edge to a node we refused would draw a line to nowhere.
            guard known.contains(candidate.source), known.contains(candidate.target) else { continue }
            appendedEdges.append(candidate)
            edgeKeys.insert(candidate.id)
        }

        nodes.append(contentsOf: appendedNodes)
        edges.append(contentsOf: appendedEdges)
        mergeDropped += dropped
        if let meta, meta.truncated {
            expansionDrops[origin] = meta.hubs_collapsed + meta.nodes_over_cap
        }
        rebuild(preservePositions: true)
    }

    // ── Control mutations ──────────────────────────────────────────────────

    func setDepth(_ value: Int, using api: API) async {
        let clamped = max(1, min(3, value))
        guard clamped != depth else { return }
        depth = clamped
        await load(using: api)
    }

    func toggleRelType(_ value: String, using api: API) async {
        if relTypes.contains(value) { relTypes.remove(value) } else { relTypes.insert(value) }
        await load(using: api)
    }

    func clearRelTypes(using api: API) async {
        guard !relTypes.isEmpty else { return }
        relTypes = []
        await load(using: api)
    }

    func toggleExcludeHubs(using api: API) async {
        excludeHubs.toggle()
        await load(using: api)
    }

    // ── Selection / pinning ────────────────────────────────────────────────

    func select(index: Int?) {
        guard let index, index >= 0, index < nodes.count else {
            selected = nil
            return
        }
        selected = nodes[index].id
    }

    /// Long-press: freeze a node where it is. Pinned nodes are excluded from
    /// integration entirely, so the rest of the graph relaxes around them.
    func togglePin(index: Int) {
        guard index >= 0, index < nodes.count else { return }
        let id = nodes[index].id
        if pinned.contains(id) { pinned.remove(id) } else { pinned.insert(id) }
        // Unpinning re-opens the layout, so let it move again.
        settled = false
        iteration = min(iteration, maxIterations - 60)
    }

    func isPinned(_ id: String) -> Bool { pinned.contains(id) }

    // ── Hit testing ────────────────────────────────────────────────────────

    /// Index of the node nearest `point` (canvas/world coordinates) within
    /// `tolerance`, or nil. Ties break toward the smaller distance, so a small
    /// node sitting on top of a big one is still selectable.
    func nodeIndex(near point: CGPoint, tolerance: CGFloat) -> Int? {
        var best: Int?
        var bestDistance = CGFloat.greatestFiniteMagnitude
        for i in 0..<min(points.count, nodes.count) {
            let p = points[i]
            let d = hypot(p.x - point.x, p.y - point.y)
            let reach = max(tolerance, radius(at: i) + 8)
            if d < bestDistance && d <= reach {
                bestDistance = d
                best = i
            }
        }
        return best
    }

    // ── Visual scales ──────────────────────────────────────────────────────

    /// Node radius by mention count, on a sqrt scale so a 4,000-mention entity
    /// doesn't become a disc that eats the canvas.
    func radius(at index: Int) -> CGFloat {
        guard index >= 0, index < nodes.count else { return 6 }
        let node = nodes[index]
        let share = Double(max(0, node.mention_count)) / Double(max(1, maxMentions))
        let clamped = max(0.0, min(1.0, share))
        let base = 5.0 + 11.0 * clamped.squareRoot()
        return CGFloat(node.id == rootId ? base + 3.0 : base)
    }

    /// Stroke width. `lift` beats raw `weight` whenever the correlation pod has
    /// scored the edge: a co-occurrence count says "these appear together a
    /// lot", which for two common entities is not news. Lift says "far more
    /// often than chance predicts", which is the actual finding.
    func width(for edge: GraphEdge) -> CGFloat {
        if let lift = edge.lift, lift > 0 {
            return CGFloat(min(5.0, 1.0 + min(lift, 60.0) * 0.07))
        }
        let denominator = max(0.0001, maxWeight)
        return CGFloat(0.8 + 2.6 * min(1.0, edge.weight / denominator))
    }

    /// Label for an edge whose lift is high enough to be worth reading. Only
    /// strong edges get one — labelling everything is the same as labelling
    /// nothing.
    static func liftLabel(for edge: GraphEdge) -> String? {
        guard let lift = edge.lift, lift >= 5 else { return nil }
        return "\(Int(lift.rounded()))× baseline"
    }

    /// Stable colour bucket for an entity type. FNV-1a rather than Swift's
    /// `hashValue`, which is seeded per process — a node that changes colour
    /// between launches is a node the analyst re-learns every time.
    static func colorBucket(forType type: String) -> Int {
        var h: UInt64 = 0xcbf2_9ce4_8422_2325
        for byte in type.utf8 {
            h = (h ^ UInt64(byte)) &* 0x0000_0100_0000_01b3
        }
        return Int(h % 6)
    }

    static func relationshipLabel(_ relationship: String) -> String {
        switch relationship {
        case "asserted":         return "Asserted"
        case "co_mention":       return "Co-mention"
        case "pivot_discovered": return "Pivot-discovered"
        default:                 return relationship
        }
    }

    // ── Layout ─────────────────────────────────────────────────────────────

    /// Bounding box of the laid-out graph, for the "Fit" control.
    func contentBounds() -> CGRect? {
        guard !points.isEmpty else { return nil }
        var minX = CGFloat.greatestFiniteMagnitude, minY = CGFloat.greatestFiniteMagnitude
        var maxX = -CGFloat.greatestFiniteMagnitude, maxY = -CGFloat.greatestFiniteMagnitude
        for p in points {
            minX = Swift.min(minX, p.x); minY = Swift.min(minY, p.y)
            maxX = Swift.max(maxX, p.x); maxY = Swift.max(maxY, p.y)
        }
        guard maxX > minX, maxY > minY else { return nil }
        return CGRect(x: minX, y: minY, width: maxX - minX, height: maxY - minY)
    }

    /// The canvas resized (rotation, split view). Shift everything by the
    /// change in centre and let it relax briefly rather than re-seeding, which
    /// would throw away a layout the user has already read.
    func updateCanvas(_ size: CGSize) {
        guard size.width > 1, size.height > 1 else { return }
        guard abs(size.width - canvas.width) > 1 || abs(size.height - canvas.height) > 1 else { return }
        let dx = (size.width - canvas.width) / 2
        let dy = (size.height - canvas.height) / 2
        canvas = size
        guard !px.isEmpty else { return }
        for i in 0..<px.count { px[i] += dx; py[i] += dy }
        publishPoints()
        settled = false
        iteration = Swift.min(iteration, maxIterations - 80)
    }

    /// Advance the simulation. Called once per animation frame from the view;
    /// a no-op the moment the layout has settled, which is what lets the
    /// `TimelineView` schedule actually pause.
    func tick() {
        guard !settled, !px.isEmpty else { return }
        var largestStep: CGFloat = 0
        for _ in 0..<stepsPerTick {
            largestStep = integrate()
            iteration += 1
            if iteration >= maxIterations { break }
        }
        publishPoints()
        if iteration >= maxIterations || largestStep < settleThreshold {
            settled = true
        }
    }

    /// Fewer integration steps per frame as the graph grows, so the per-frame
    /// cost stays roughly flat instead of scaling with n².
    private var stepsPerTick: Int {
        if px.count > 160 { return 1 }
        if px.count > 60  { return 2 }
        return 3
    }

    private func publishPoints() {
        var out = [CGPoint](repeating: .zero, count: px.count)
        for i in 0..<px.count { out[i] = CGPoint(x: px[i], y: py[i]) }
        points = out
    }

    /// Rebuild the index, the link list and the seed positions. `preservePositions`
    /// keeps every node that was already on screen exactly where it was, so a
    /// merge grows the picture instead of reshuffling it.
    private func rebuild(preservePositions: Bool) {
        var previous: [String: CGPoint] = [:]
        if preservePositions {
            for (i, id) in laidOutIDs.enumerated() where i < px.count {
                previous[id] = CGPoint(x: px[i], y: py[i])
            }
        }

        let count = nodes.count
        var index: [String: Int] = [:]
        index.reserveCapacity(count)
        for (i, node) in nodes.enumerated() { index[node.id] = i }

        px = [CGFloat](repeating: 0, count: count)
        py = [CGFloat](repeating: 0, count: count)
        vx = [CGFloat](repeating: 0, count: count)
        vy = [CGFloat](repeating: 0, count: count)
        laidOutIDs = nodes.map { $0.id }

        let cx = canvas.width / 2
        let cy = canvas.height / 2
        let ring = Swift.max(60, Swift.min(canvas.width, canvas.height) * 0.34)
        for (i, node) in nodes.enumerated() {
            if let old = previous[node.id] {
                px[i] = old.x; py[i] = old.y
            } else if node.id == rootId {
                px[i] = cx; py[i] = cy
            } else {
                // Golden-angle spiral seed: neighbours in the array don't land
                // on top of each other, which the plain `i/n * 2π` ring does
                // whenever a merge appends a run of related nodes.
                let angle = Double(i) * 2.399963
                let spread = ring * CGFloat((Double(i) / Double(Swift.max(1, count))).squareRoot() + 0.25)
                px[i] = cx + CGFloat(cos(angle)) * spread
                py[i] = cy + CGFloat(sin(angle)) * spread
            }
        }

        var built: [GraphLink] = []
        built.reserveCapacity(edges.count)
        for edge in edges {
            guard let a = index[edge.source], let b = index[edge.target], a != b else { continue }
            built.append(GraphLink(edge: edge, a: a, b: b))
        }
        links = built

        maxMentions = Swift.max(1, nodes.map { $0.mention_count }.max() ?? 1)
        maxWeight = Swift.max(0.0001, edges.map { $0.weight }.max() ?? 1)

        publishPoints()
        iteration = 0
        settled = count <= 1
    }

    /// One integration step. Returns the largest single-node displacement, which
    /// is the convergence signal.
    private func integrate() -> CGFloat {
        let n = px.count
        guard n > 1 else { return 0 }

        let cutoffSquared = repulsionCutoff * repulsionCutoff

        // Pairwise repulsion, with a distance cutoff. Once the graph has spread
        // out most pairs are beyond it and cost only a subtraction and a compare.
        var i = 0
        while i < n {
            var j = i + 1
            while j < n {
                var dx = px[i] - px[j]
                var dy = py[i] - py[j]
                let d2 = dx * dx + dy * dy
                if d2 < cutoffSquared {
                    let safe = Swift.max(d2, 0.01)
                    let d = safe.squareRoot()
                    let f = repulsion / safe
                    dx /= d; dy /= d
                    vx[i] += dx * f; vy[i] += dy * f
                    vx[j] -= dx * f; vy[j] -= dy * f
                }
                j += 1
            }
            i += 1
        }

        // Springs along edges.
        for link in links {
            let a = link.a, b = link.b
            let dx = px[b] - px[a]
            let dy = py[b] - py[a]
            let d = Swift.max((dx * dx + dy * dy).squareRoot(), 0.01)
            let f = (d - springRest) * springStiffness
            let ux = dx / d, uy = dy / d
            vx[a] += ux * f; vy[a] += uy * f
            vx[b] -= ux * f; vy[b] -= uy * f
        }

        // Integrate, damp, clamp to the canvas. The root and any pinned node
        // are held still — pinning is the user's way of saying "this is my
        // anchor, relax everything else around it".
        let margin: CGFloat = 26
        let maxX = Swift.max(margin + 1, canvas.width - margin)
        let maxY = Swift.max(margin + 1, canvas.height - margin)
        var largestStep: CGFloat = 0
        var k = 0
        while k < n {
            let id = laidOutIDs[k]
            if id == rootId || pinned.contains(id) {
                vx[k] = 0; vy[k] = 0
                k += 1
                continue
            }
            vx[k] *= damping
            vy[k] *= damping
            // Hard velocity clamp: a pair that starts nearly coincident can
            // otherwise produce a single enormous step and fling a node off.
            vx[k] = Swift.max(-40, Swift.min(40, vx[k]))
            vy[k] = Swift.max(-40, Swift.min(40, vy[k]))
            let step = hypot(vx[k], vy[k])
            if step > largestStep { largestStep = step }
            px[k] = Swift.max(margin, Swift.min(maxX, px[k] + vx[k]))
            py[k] = Swift.max(margin, Swift.min(maxY, py[k] + vy[k]))
            k += 1
        }
        return largestStep
    }

    // ── Errors ─────────────────────────────────────────────────────────────

    static func explain(_ error: Error) -> String {
        guard let apiError = error as? APIError,
              case .http(let code, let body) = apiError else {
            return error.localizedDescription
        }
        if code == 404 {
            return "That entity no longer exists, or its type doesn't match the one on record."
        }
        if code == 401 || code == 403 {
            return "You're not signed in, or this workspace doesn't grant entity access."
        }
        return "HTTP \(code): \(body.prefix(200))"
    }
}
