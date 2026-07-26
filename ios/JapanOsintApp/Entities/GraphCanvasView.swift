import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 19 — entity graph canvas.
//
// Force-directed layout on a `Canvas`, stepped once per frame by
// `SwiftUI.TimelineView(.animation(paused:))` and PAUSED the moment the layout
// settles — the integrator is bounded, so this never becomes a view that spins
// a core forever behind a picture that stopped moving a minute ago.
//
// Interactions all come out of ONE `DragGesture(minimumDistance: 0)` plus a
// magnify gesture. Deliberately: layering `.onTapGesture(count: 2)`,
// `.onTapGesture`, `.onLongPressGesture` and a pan `DragGesture` on the same
// view is a gesture-arbitration coin flip, and `onLongPressGesture` does not
// hand back the press location, which pinning needs. Classifying one drag by
// distance and duration is boring, deterministic, and gives every interaction
// the point it happened at:
//
//   move > 6pt                  → pan
//   press ≥ 0.5s, then lift     → toggle pin on the node under the finger
//   quick tap                   → select
//   second quick tap, same node → expand (fetch that node's ego-network, merge)
//
// The one behavioural consequence worth knowing: the pin lands on finger-lift,
// not at the 0.5s mark, because a perfectly still finger emits no further drag
// events to fire on.
//
// ── The banner is the point ─────────────────────────────────────────────────
// `GraphMeta.truncated` is rendered loudly and never conditionally hidden. A
// canvas that silently drops half a graph teaches an analyst to trust a picture
// that is lying to them — and the lie is expensive here, because "no edge
// between these two" is exactly the kind of negative finding people act on.
// ─────────────────────────────────────────────────────────────────────────────

struct GraphCanvasView: View {
    let entityType: String
    let entityId: String
    /// Optional push handler for the selection chip's "Open" action. When nil
    /// the button is simply absent — in-canvas expansion still works, so the
    /// view is fully usable without a navigation host.
    var onOpenEntity: ((GraphNode) -> Void)? = nil
    /// Seam for "Add visible to case". Cases UI is owned elsewhere; this view
    /// deliberately imports none of it. Unwired, the action explains itself
    /// rather than failing silently.
    var onAddVisibleToCase: (([GraphNode]) -> Void)? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @StateObject private var model: GraphCanvasModel

    // Viewport
    @State private var canvasSize: CGSize = .zero
    @State private var scale: CGFloat = 1
    @State private var liveScale: CGFloat = 1
    @State private var offset: CGSize = .zero
    @State private var liveOffset: CGSize = .zero

    // Gesture classification
    @State private var pressStartedAt: Date?
    @State private var panning = false
    @State private var lastTapAt: Date?
    @State private var lastTapIndex: Int?

    @State private var showCaseNotice = false

    private let tapSlop: CGFloat = 6
    private let longPressSeconds: TimeInterval = 0.5
    private let doubleTapSeconds: TimeInterval = 0.42

    init(entityType: String,
         entityId: String,
         onOpenEntity: ((GraphNode) -> Void)? = nil,
         onAddVisibleToCase: (([GraphNode]) -> Void)? = nil) {
        self.entityType = entityType
        self.entityId = entityId
        self.onOpenEntity = onOpenEntity
        self.onAddVisibleToCase = onAddVisibleToCase
        _model = StateObject(wrappedValue: GraphCanvasModel(rootType: entityType,
                                                            rootId: entityId))
    }

    var body: some View {
        VStack(spacing: Space.sm) {
            controlBar
            if model.isTruncated, let summary = model.truncationSummary {
                truncationBanner(summary)
            }
            if let error = model.error {
                errorBanner(error)
            }
            ZStack(alignment: .bottom) {
                canvasArea
                if let node = model.selectedNode {
                    detailChip(node)
                        .padding(Space.sm)
                        .transition(.move(edge: .bottom).combined(with: .opacity))
                }
            }
            legendRow
        }
        .padding(.horizontal, Space.md)
        .padding(.vertical, Space.sm)
        .compatReadableWidth()
        .animation(.spring(response: 0.3, dampingFraction: 0.85), value: model.selected)
        .task {
            if model.nodes.isEmpty { await model.load(using: apiClient.api) }
        }
        .alert("Not wired yet",
               isPresented: $showCaseNotice) {
            Button("OK", role: .cancel) { showCaseNotice = false }
        } message: {
            Text("\"Add visible to case\" needs a case picker, which lives in the Cases UI. "
                 + "Pass an `onAddVisibleToCase` handler into GraphCanvasView to enable it.")
        }
        .sensoryFeedback(.selection, trigger: model.depth)
    }

    // ── Controls ───────────────────────────────────────────────────────────

    private var controlBar: some View {
        VStack(spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                Text("Depth")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                Picker("Depth", selection: depthBinding) {
                    ForEach(1...3, id: \.self) { value in
                        Text("\(value)").tag(value)
                    }
                }
                .pickerStyle(.segmented)
                .frame(maxWidth: 130)
                .disabled(model.loading)
                Spacer(minLength: 0)
                if model.loading {
                    ProgressView().controlSize(.mini)
                }
                relationshipMenu
                actionsMenu
            }
            HStack(spacing: Space.sm) {
                Toggle(isOn: hubBinding) {
                    Text("Collapse hubs (>\(GraphCanvasModel.hubDegreeThreshold) edges)")
                        .font(.caption2)
                        .foregroundStyle(theme.text)
                }
                .toggleStyle(.switch)
                .disabled(model.loading)
                Spacer(minLength: 0)
                zoomButton("minus.magnifyingglass", "Zoom out") { zoom(by: 1 / 1.35) }
                zoomButton("plus.magnifyingglass", "Zoom in") { zoom(by: 1.35) }
                zoomButton("arrow.up.left.and.arrow.down.right", "Fit graph") { fitToContent() }
            }
        }
    }

    private func zoomButton(_ symbol: String, _ label: String,
                            action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: symbol)
                .font(.caption)
                .foregroundStyle(theme.accent)
                .frame(width: 28, height: 28)
                .background(theme.accent.opacity(0.12), in: Circle())
        }
        .frame(minWidth: 34, minHeight: 34)
        .contentShape(Rectangle())
        .buttonStyle(.plain)
        .accessibilityLabel(label)
    }

    private var relationshipMenu: some View {
        Menu {
            Button {
                Task { await model.clearRelTypes(using: apiClient.api) }
            } label: {
                if model.relTypes.isEmpty {
                    Label("All relationships", systemImage: "checkmark")
                } else {
                    Text("All relationships")
                }
            }
            Divider()
            ForEach(GraphCanvasModel.relTypeOptions, id: \.self) { relationship in
                Button {
                    Task { await model.toggleRelType(relationship, using: apiClient.api) }
                } label: {
                    if model.relTypes.contains(relationship) {
                        Label(GraphCanvasModel.relationshipLabel(relationship),
                              systemImage: "checkmark")
                    } else {
                        Text(GraphCanvasModel.relationshipLabel(relationship))
                    }
                }
            }
        } label: {
            chipLabel(
                icon: "line.3.horizontal.decrease",
                text: model.relTypes.isEmpty ? "All rels" : "\(model.relTypes.count) rels",
                active: !model.relTypes.isEmpty
            )
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Relationship type filter")
    }

    private var actionsMenu: some View {
        Menu {
            Button {
                if let handler = onAddVisibleToCase {
                    handler(model.nodes)
                    Haptics.success()
                } else {
                    showCaseNotice = true
                }
            } label: {
                Label("Add visible to case (\(model.nodes.count))", systemImage: "folder.badge.plus")
            }
            .disabled(model.nodes.isEmpty)
            Divider()
            Button {
                Task { await model.load(using: apiClient.api) }
            } label: {
                Label("Reload graph", systemImage: "arrow.clockwise")
            }
            Button {
                resetViewport()
            } label: {
                Label("Reset zoom & pan", systemImage: "scope")
            }
        } label: {
            chipLabel(icon: "ellipsis.circle", text: "Actions", active: false)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Graph actions")
    }

    private func chipLabel(icon: String, text: String, active: Bool) -> some View {
        HStack(spacing: 3) {
            Image(systemName: icon).font(.caption2)
            Text(text).font(.caption.weight(.semibold)).lineLimit(1)
        }
        .foregroundStyle(active ? theme.accent : theme.textMuted)
        .padding(.horizontal, Space.sm)
        .padding(.vertical, 5)
        .background(Capsule().fill(active ? theme.accent.opacity(0.16) : theme.surfaceElevated))
        .overlay(
            Capsule().stroke(active ? theme.accent.opacity(0.6) : theme.textMuted.opacity(0.3),
                             lineWidth: Stroke.hairline)
        )
    }

    private var depthBinding: Binding<Int> {
        Binding(
            get: { model.depth },
            set: { value in Task { await model.setDepth(value, using: apiClient.api) } }
        )
    }

    private var hubBinding: Binding<Bool> {
        Binding(
            get: { model.excludeHubs },
            set: { _ in Task { await model.toggleExcludeHubs(using: apiClient.api) } }
        )
    }

    // ── Banners ────────────────────────────────────────────────────────────

    private func truncationBanner(_ summary: String) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(alignment: .top, spacing: Space.sm) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(theme.warning)
                Text(summary)
                    .font(.caption.weight(.bold).monospacedDigit())
                    .foregroundStyle(theme.text)
                    .fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 0)
            }
            ForEach(model.truncationReasons, id: \.self) { reason in
                Text("· " + reason)
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Text("A missing edge here is not evidence that no relationship exists.")
                .font(.caption2.italic())
                .foregroundStyle(theme.warning)
                .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Space.sm)
        .background(theme.warning.opacity(0.14),
                    in: RoundedRectangle(cornerRadius: Radius.sm))
        .overlay(
            RoundedRectangle(cornerRadius: Radius.sm)
                .stroke(theme.warning.opacity(0.55), lineWidth: Stroke.regular)
        )
        .accessibilityElement(children: .combine)
    }

    private func errorBanner(_ text: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: "xmark.octagon.fill")
                .font(.caption)
                .foregroundStyle(theme.danger)
            Text(text)
                .font(.caption2)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
            Button("Retry") {
                Task { await model.load(using: apiClient.api) }
            }
            .font(.caption2.weight(.semibold))
            .buttonStyle(.plain)
            .foregroundStyle(theme.accent)
        }
        .padding(Space.sm)
        .background(theme.danger.opacity(0.12),
                    in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    // ── Canvas ─────────────────────────────────────────────────────────────

    private var canvasArea: some View {
        GeometryReader { geo in
            // Module-qualified: `TimelineView` is SwiftUI's, and this app also
            // ships a timeline screen next door. Qualifying removes any doubt
            // about which one this is.
            SwiftUI.TimelineView(.animation(minimumInterval: 1.0 / 60.0,
                                            paused: model.settled)) { timeline in
                Canvas { context, size in
                    draw(&context, size: size)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                // One integration step batch per frame. `paused: model.settled`
                // stops the schedule entirely once the layout converges, so a
                // settled graph costs nothing.
                .onChange(of: timeline.date) { _, _ in
                    model.tick()
                }
            }
            .contentShape(Rectangle())
            .gesture(panTapPressGesture)
            .simultaneousGesture(zoomGesture)
            .onAppear {
                canvasSize = geo.size
                model.updateCanvas(geo.size)
            }
            .onChange(of: geo.size) { _, newSize in
                canvasSize = newSize
                model.updateCanvas(newSize)
            }
            .overlay {
                if model.nodes.isEmpty && !model.loading {
                    emptyState
                }
            }
        }
        .frame(minHeight: 340)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .clipShape(RoundedRectangle(cornerRadius: Radius.md))
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No relationships", systemImage: "circle.dashed")
                .foregroundStyle(theme.text)
        } description: {
            Text("This entity has no recorded relationships at depth \(model.depth)"
                 + (model.relTypes.isEmpty ? "." : " for the selected relationship types."))
                .foregroundStyle(theme.textMuted)
        }
    }

    /// The whole render pass. Reads only published state — it never mutates the
    /// model, so a redraw can never re-enter the simulation.
    private func draw(_ context: inout GraphicsContext, size: CGSize) {
        let points = model.points
        let nodes = model.nodes
        guard !points.isEmpty, points.count == nodes.count else { return }
        let zoom = totalScale
        let labelsVisible = nodes.count <= 60 || zoom >= 1.6

        // Edges first so node discs sit on top of their own connections.
        for link in model.links {
            guard link.a < points.count, link.b < points.count else { continue }
            let a = screenPoint(points[link.a], in: size)
            let b = screenPoint(points[link.b], in: size)
            let edge = link.edge
            let color = edgeColor(edge)
            let lineWidth = model.width(for: edge)

            var path = Path()
            path.move(to: a)
            path.addLine(to: b)

            if edge.relationship == "co_mention" {
                // Dashed: co-occurrence is circumstantial. It should not look
                // like the same class of claim as an asserted relationship.
                context.stroke(path, with: .color(color),
                               style: StrokeStyle(lineWidth: lineWidth, lineCap: .round,
                                                  dash: [5, 4]))
            } else {
                context.stroke(path, with: .color(color),
                               style: StrokeStyle(lineWidth: lineWidth, lineCap: .round))
            }

            if edge.relationship == "pivot_discovered" {
                // Directed: a pivot went FROM source TO target. The arrow is
                // the whole point — reversing it inverts the finding.
                let inset = model.radius(at: link.b) * zoom + 2
                drawArrowHead(&context, from: a, to: b, color: color,
                              lineWidth: lineWidth, inset: inset)
            }

            if labelsVisible, let label = GraphCanvasModel.liftLabel(for: edge) {
                let mid = CGPoint(x: (a.x + b.x) / 2, y: (a.y + b.y) / 2)
                context.draw(
                    Text(label)
                        .font(.system(size: 8, weight: .semibold, design: .monospaced))
                        .foregroundColor(theme.warning),
                    at: CGPoint(x: mid.x, y: mid.y - 7)
                )
            }
        }

        for index in 0..<nodes.count {
            let node = nodes[index]
            let center = screenPoint(points[index], in: size)
            let radius = model.radius(at: index) * zoom
            // Cheap cull — a 300-node graph zoomed into one cluster shouldn't
            // pay to draw the 280 discs that are off screen.
            guard center.x > -radius - 40, center.x < size.width + radius + 40,
                  center.y > -radius - 40, center.y < size.height + radius + 40 else { continue }

            let rect = CGRect(x: center.x - radius, y: center.y - radius,
                              width: radius * 2, height: radius * 2)
            let color = typeColor(node.type)
            context.fill(Path(ellipseIn: rect), with: .color(color))

            if node.id == model.rootId {
                context.stroke(Path(ellipseIn: rect.insetBy(dx: -3, dy: -3)),
                               with: .color(theme.text), lineWidth: 1.5)
            }
            if model.isPinned(node.id) {
                context.stroke(Path(ellipseIn: rect.insetBy(dx: -2, dy: -2)),
                               with: .color(theme.warning),
                               style: StrokeStyle(lineWidth: 1.5, dash: [2.5, 2.5]))
            }
            if model.selected == node.id {
                context.stroke(Path(ellipseIn: rect.insetBy(dx: -5, dy: -5)),
                               with: .color(theme.accent), lineWidth: 2)
            }
            if model.expanded.contains(node.id) {
                let tick = CGRect(x: rect.maxX - 3, y: rect.minY - 3, width: 6, height: 6)
                context.fill(Path(ellipseIn: tick), with: .color(theme.success))
            }

            if labelsVisible || node.id == model.rootId || model.selected == node.id {
                let text = node.label.isEmpty ? node.value : node.label
                context.draw(
                    Text(String(text.prefix(22)))
                        .font(.system(size: 9))
                        .foregroundColor(theme.text),
                    at: CGPoint(x: center.x, y: center.y - radius - 8)
                )
            }
        }
    }

    private func drawArrowHead(_ context: inout GraphicsContext,
                               from a: CGPoint, to b: CGPoint,
                               color: Color, lineWidth: CGFloat, inset: CGFloat) {
        let dx = b.x - a.x
        let dy = b.y - a.y
        let distance = max(hypot(dx, dy), 0.01)
        let ux = dx / distance
        let uy = dy / distance
        let tip = CGPoint(x: b.x - ux * inset, y: b.y - uy * inset)
        let headLength = max(7, lineWidth * 3)
        let baseX = tip.x - ux * headLength
        let baseY = tip.y - uy * headLength
        let nx = -uy
        let ny = ux
        let halfWidth = headLength * 0.45
        var head = Path()
        head.move(to: tip)
        head.addLine(to: CGPoint(x: baseX + nx * halfWidth, y: baseY + ny * halfWidth))
        head.addLine(to: CGPoint(x: baseX - nx * halfWidth, y: baseY - ny * halfWidth))
        head.closeSubpath()
        context.fill(head, with: .color(color))
    }

    // ── Legend ─────────────────────────────────────────────────────────────

    private var legendRow: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: Space.sm) {
                    ForEach(model.typesPresent, id: \.self) { type in
                        HStack(spacing: 4) {
                            Circle().fill(typeColor(type)).frame(width: 8, height: 8)
                            Text(type).font(.caption2).foregroundStyle(theme.textMuted)
                        }
                    }
                }
            }
            HStack(spacing: Space.md) {
                edgeKey("Asserted", dashed: false, arrow: false)
                edgeKey("Co-mention", dashed: true, arrow: false)
                edgeKey("Pivot", dashed: false, arrow: true)
                Spacer(minLength: 0)
                Text("size = mentions")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    private func edgeKey(_ label: String, dashed: Bool, arrow: Bool) -> some View {
        HStack(spacing: 4) {
            Canvas { context, size in
                var path = Path()
                path.move(to: CGPoint(x: 0, y: size.height / 2))
                path.addLine(to: CGPoint(x: size.width - (arrow ? 5 : 0), y: size.height / 2))
                context.stroke(
                    path,
                    with: .color(theme.textMuted),
                    style: StrokeStyle(lineWidth: 1.4, lineCap: .round,
                                       dash: dashed ? [3, 2.5] : [])
                )
                if arrow {
                    var head = Path()
                    head.move(to: CGPoint(x: size.width, y: size.height / 2))
                    head.addLine(to: CGPoint(x: size.width - 5, y: size.height / 2 - 3))
                    head.addLine(to: CGPoint(x: size.width - 5, y: size.height / 2 + 3))
                    head.closeSubpath()
                    context.fill(head, with: .color(theme.textMuted))
                }
            }
            .frame(width: 20, height: 10)
            Text(label).font(.caption2).foregroundStyle(theme.textMuted)
        }
    }

    // ── Selection chip ─────────────────────────────────────────────────────

    private func detailChip(_ node: GraphNode) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                Circle().fill(typeColor(node.type)).frame(width: 10, height: 10)
                Text(node.label.isEmpty ? node.value : node.label)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Spacer(minLength: 0)
                Button {
                    model.select(index: nil)
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(theme.textMuted)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Clear selection")
            }
            HStack(spacing: Space.xs) {
                Pill(text: node.type, tone: .info, maxWidth: 120)
                Pill(text: "\(node.mention_count) mentions", tone: .neutral, maxWidth: 140)
                Pill(text: "\(model.edgeCount(for: node.id)) edges", tone: .neutral, maxWidth: 110)
                if model.isPinned(node.id) {
                    Pill(text: "PINNED", tone: .warning, icon: "pin.fill", maxWidth: 100)
                }
                Spacer(minLength: 0)
            }
            HStack(spacing: Space.md) {
                if model.expanding == node.id {
                    ProgressView().controlSize(.mini)
                    Text("Expanding…").font(.caption2).foregroundStyle(theme.textMuted)
                } else if model.expanded.contains(node.id) {
                    Label("Expanded", systemImage: "checkmark.circle")
                        .font(.caption2)
                        .foregroundStyle(theme.success)
                } else {
                    Button("Expand") {
                        Task { await model.expand(node.id, using: apiClient.api) }
                    }
                    .font(.caption.weight(.semibold))
                    .buttonStyle(.plain)
                    .foregroundStyle(theme.accent)
                }
                Button(model.isPinned(node.id) ? "Unpin" : "Pin") {
                    if let index = model.nodes.firstIndex(where: { $0.id == node.id }) {
                        model.togglePin(index: index)
                        Haptics.selection()
                    }
                }
                .font(.caption.weight(.semibold))
                .buttonStyle(.plain)
                .foregroundStyle(theme.warning)
                if let open = onOpenEntity {
                    Button("Open") { open(node) }
                        .font(.caption.weight(.semibold))
                        .buttonStyle(.plain)
                        .foregroundStyle(theme.accentAlt)
                }
                Spacer(minLength: 0)
            }
            Text("Double-tap a node to expand · press and hold to pin · drag to pan · pinch to zoom")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(Space.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.thinMaterial, in: RoundedRectangle(cornerRadius: Radius.md))
        .overlay(
            RoundedRectangle(cornerRadius: Radius.md)
                .stroke(theme.accent.opacity(0.35), lineWidth: Stroke.hairline)
        )
    }

    // ── Gestures ───────────────────────────────────────────────────────────

    private var panTapPressGesture: some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { value in
                let now = Date()
                // A gesture cancelled by a pinch never reaches `onEnded`, so a
                // stale start time would make the NEXT quick tap look like a
                // five-minute press. Anything older than 5s is treated as stale.
                if pressStartedAt == nil || now.timeIntervalSince(pressStartedAt ?? now) > 5 {
                    pressStartedAt = now
                }
                let moved = hypot(value.translation.width, value.translation.height)
                if moved > tapSlop { panning = true }
                if panning { liveOffset = value.translation }
            }
            .onEnded { value in
                let moved = hypot(value.translation.width, value.translation.height)
                let elapsed = Date().timeIntervalSince(pressStartedAt ?? Date())
                if panning || moved > tapSlop {
                    offset = CGSize(width: offset.width + value.translation.width,
                                    height: offset.height + value.translation.height)
                } else if elapsed >= longPressSeconds {
                    handleLongPress(at: value.startLocation)
                } else {
                    handleTap(at: value.location)
                }
                liveOffset = .zero
                panning = false
                pressStartedAt = nil
            }
    }

    private var zoomGesture: some Gesture {
        MagnifyGesture()
            .onChanged { value in
                liveScale = value.magnification
            }
            .onEnded { value in
                scale = clampScale(scale * value.magnification)
                liveScale = 1
            }
    }

    private func handleTap(at point: CGPoint) {
        guard canvasSize.width > 1 else { return }
        let world = worldPoint(point, in: canvasSize)
        guard let index = model.nodeIndex(near: world,
                                          tolerance: 22 / max(totalScale, 0.3)) else {
            model.select(index: nil)
            lastTapAt = nil
            lastTapIndex = nil
            return
        }
        let now = Date()
        if let previous = lastTapAt, lastTapIndex == index,
           now.timeIntervalSince(previous) < doubleTapSeconds {
            lastTapAt = nil
            lastTapIndex = nil
            let nodeId = model.nodes[index].id
            Task { await model.expand(nodeId, using: apiClient.api) }
            return
        }
        lastTapAt = now
        lastTapIndex = index
        model.select(index: index)
        Haptics.selection()
    }

    private func handleLongPress(at point: CGPoint) {
        guard canvasSize.width > 1 else { return }
        let world = worldPoint(point, in: canvasSize)
        guard let index = model.nodeIndex(near: world,
                                          tolerance: 26 / max(totalScale, 0.3)) else { return }
        model.select(index: index)
        model.togglePin(index: index)
        Haptics.warning()
    }

    // ── Viewport maths ─────────────────────────────────────────────────────

    private var totalScale: CGFloat { clampScale(scale * liveScale) }

    private var totalOffset: CGSize {
        CGSize(width: offset.width + liveOffset.width,
               height: offset.height + liveOffset.height)
    }

    private func clampScale(_ value: CGFloat) -> CGFloat {
        max(0.3, min(4.0, value))
    }

    /// Layout space → screen space. The model lays out inside the canvas rect,
    /// so the two share an origin and a centre; zoom and pan are the only
    /// difference.
    private func screenPoint(_ point: CGPoint, in size: CGSize) -> CGPoint {
        let cx = size.width / 2
        let cy = size.height / 2
        let s = totalScale
        return CGPoint(x: (point.x - cx) * s + cx + totalOffset.width,
                       y: (point.y - cy) * s + cy + totalOffset.height)
    }

    /// Inverse of `screenPoint`, for hit testing.
    private func worldPoint(_ point: CGPoint, in size: CGSize) -> CGPoint {
        let cx = size.width / 2
        let cy = size.height / 2
        let s = totalScale
        return CGPoint(x: (point.x - cx - totalOffset.width) / s + cx,
                       y: (point.y - cy - totalOffset.height) / s + cy)
    }

    private func zoom(by factor: CGFloat) {
        scale = clampScale(scale * factor)
        Haptics.selection()
    }

    private func resetViewport() {
        scale = 1
        liveScale = 1
        offset = .zero
        liveOffset = .zero
    }

    private func fitToContent() {
        guard canvasSize.width > 1, canvasSize.height > 1,
              let bounds = model.contentBounds() else {
            resetViewport()
            return
        }
        let padding: CGFloat = 56
        let sx = (canvasSize.width - padding) / max(1, bounds.width)
        let sy = (canvasSize.height - padding) / max(1, bounds.height)
        let s = clampScale(min(sx, sy))
        scale = s
        liveScale = 1
        // Put the content's centre on the canvas's centre: solving
        // (b − c)·s + c + off = c gives off = (c − b)·s.
        offset = CGSize(width: (canvasSize.width / 2 - bounds.midX) * s,
                        height: (canvasSize.height / 2 - bounds.midY) * s)
        liveOffset = .zero
        Haptics.selection()
    }

    // ── Palette ────────────────────────────────────────────────────────────

    /// Stable colour per entity type (see `GraphCanvasModel.colorBucket`).
    private func typeColor(_ type: String) -> Color {
        switch GraphCanvasModel.colorBucket(forType: type) {
        case 0: return theme.accent
        case 1: return theme.accentAlt
        case 2: return theme.success
        case 3: return theme.warning
        case 4: return theme.danger
        default: return theme.textMuted
        }
    }

    /// Edges read as structure, not as data — muted unless the edge touches the
    /// selection or carries a strong lift, both of which are worth the eye.
    private func edgeColor(_ edge: GraphEdge) -> Color {
        if let selected = model.selected,
           edge.source == selected || edge.target == selected {
            return theme.accent.opacity(0.95)
        }
        if let lift = edge.lift, lift >= 5 {
            return theme.warning.opacity(0.8)
        }
        switch edge.relationship {
        case "asserted":         return theme.text.opacity(0.45)
        case "pivot_discovered": return theme.accentAlt.opacity(0.55)
        default:                 return theme.textMuted.opacity(0.5)
        }
    }
}
