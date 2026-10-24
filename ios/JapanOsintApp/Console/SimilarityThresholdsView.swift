import SwiftUI

/// Operator screen for the server's embedding-similarity thresholds —
/// `GET /api/admin/thresholds` and `PUT /api/admin/thresholds/:key`
/// (core/thresholds.c). Each row shows the value in force, where it came from
/// (runtime override, environment variable, or compiled default), the default
/// and the range; the operator moves it with a slider or types it, then saves
/// or resets. A saved value applies on the server's next comparison with no
/// restart, and the server writes every change to the audit chain.
///
/// Every route is behind `opgate_check`; the `isPlatformAdmin` check here is
/// presentation only (same stance as `AdminPanel` / `BreachCorpusView`).
/// Pushed inside Console's / RootView's stack, so no `NavigationStack`.
struct SimilarityThresholdsView: View {
    @EnvironmentObject var auth: AuthSession
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    private var api: API { apiClient.api }

    @State private var rows: [SimilarityThreshold] = []
    @State private var order: String?
    @State private var loading = false
    @State private var loaded = false
    @State private var errorText: String?

    var body: some View {
        Group {
            if auth.isPlatformAdmin { content } else { denied }
        }
        .navigationTitle("Thresholds")
        .compatInlineTitle()
        .themedScreenBackground(theme)
        .task { await load() }
        .refreshable { await load() }
    }

    private var denied: some View {
        ContentUnavailableView(
            "Operator access only",
            systemImage: "lock.shield",
            description: Text("Similarity thresholds are restricted to the platform operator account.")
        )
    }

    private var content: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                Text("Every embedding cut-off the server judges a vector comparison against. Resolution is \(order ?? "db > env > default"); a saved value applies on the next comparison, with no restart, and is recorded in the audit log. None has been measured on the production model.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                if let errorText {
                    HStack(alignment: .top, spacing: Space.sm) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .font(.caption).foregroundStyle(theme.danger)
                            .accessibilityHidden(true)   // the text follows
                        Text(errorText).font(.caption).foregroundStyle(theme.text)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .textSelection(.enabled)
                    }
                    .padding(Space.sm)
                    .background(theme.danger.opacity(0.12),
                                in: RoundedRectangle(cornerRadius: Radius.sm))
                }
                if loading && !loaded {
                    ProgressView().frame(maxWidth: .infinity)
                } else if loaded && rows.isEmpty && errorText == nil {
                    ContentUnavailableView("No thresholds",
                                           systemImage: "slider.horizontal.3",
                                           description: Text("The server registers no thresholds."))
                }
                ForEach(rows) { t in
                    SimilarityThresholdRow(threshold: t) { updated in
                        if let i = rows.firstIndex(where: { $0.key == updated.key }) {
                            rows[i] = updated
                        }
                    }
                }
            }
            .padding(Space.lg)
            .frame(maxWidth: 720, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .top)
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await api.similarityThresholds()
            rows = env.data
            order = env.meta?.order
            errorText = nil
        } catch {
            errorText = API.thresholdErrorMessage(error)
        }
        loaded = true
    }
}

/// One threshold: value, provenance, slider + text field, save / reset.
private struct SimilarityThresholdRow: View {
    let threshold: SimilarityThreshold
    let onSaved: (SimilarityThreshold) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var draft: Double = 0
    @State private var text: String = ""
    @State private var busy = false
    @State private var errorText: String?

    private var t: SimilarityThreshold { threshold }

    /// The typed value when it parses and lies in the server's range.
    private var typed: Double? {
        guard let v = Double(text.trimmingCharacters(in: .whitespaces)),
              v.isFinite, v >= t.min, v <= t.max else { return nil }
        return v
    }
    private var dirty: Bool { typed.map { $0 != t.value } ?? false }
    private var step: Double { (t.max - t.min) <= 0.2 ? 0.001 : 0.01 }

    private var sourceLabel: String {
        switch t.source {
        case "db": return "runtime override"
        case "env": return "environment"
        default: return "default"
        }
    }
    private var sourceTone: Pill.Tone {
        switch t.source {
        case "db": return .accent
        case "env": return .warning
        default: return .neutral
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                Text(t.key)
                    .font(.system(.caption, design: .monospaced).weight(.semibold))
                    .foregroundStyle(theme.text)
                Pill(text: sourceLabel, tone: sourceTone)
                if let m = t.metric { Pill(text: m, tone: .neutral) }
                Spacer(minLength: 0)
            }
            if let d = t.description {
                Text(d).font(.caption2).foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Text("default \(fmt(t.defaultValue)) · range [\(fmt(t.min)), \(fmt(t.max))] · \(t.env)\(t.envValue.map { "=\(fmt($0))" } ?? " unset")")
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)

            HStack(spacing: Space.sm) {
                Slider(value: Binding(
                    get: { draft },
                    set: { v in
                        let r = (v / step).rounded() * step
                        draft = Swift.min(Swift.max(r, t.min), t.max)
                        text = fmt(draft)
                    }), in: t.min...t.max)
                    .accessibilityLabel(t.key)
                    .accessibilityValue(fmt(draft))
                TextField("value", text: $text)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(.caption, design: .monospaced))
                    .frame(width: 80)
                    .autocorrectionDisabled()
                    #if os(iOS)
                    .keyboardType(.decimalPad)
                    #endif
                    .onChange(of: text) { _, new in
                        if let v = Double(new), v.isFinite, v >= t.min, v <= t.max { draft = v }
                    }
            }
            if typed == nil && text != fmt(t.value) {
                Text("Must be a number between \(fmt(t.min)) and \(fmt(t.max))")
                    .font(.caption2).foregroundStyle(theme.danger)
            }

            HStack(spacing: Space.sm) {
                Button {
                    Haptics.tap()
                    if let v = typed { Task { await save(v) } }
                } label: {
                    HStack(spacing: 6) {
                        if busy { ProgressView().controlSize(.small) }
                        else { Image(systemName: "checkmark").accessibilityHidden(true) }
                        Text("Save")
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(!dirty || busy)

                Button {
                    Haptics.tap()
                    Task { await reset() }
                } label: {
                    Label("Reset", systemImage: "arrow.uturn.backward")
                }
                .buttonStyle(.bordered)
                .disabled(!t.isOverridden || busy)
                Spacer(minLength: 0)
            }
            if t.isOverridden, let by = t.updatedBy {
                Text("override by \(by)\(t.updatedAt.map { " · \($0)" } ?? "")")
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
            }
            if let errorText {
                Text(errorText).font(.caption).foregroundStyle(theme.warning)
                    .textSelection(.enabled)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Space.md)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .onAppear { sync() }
        .onChange(of: threshold) { _, _ in sync() }
    }

    private func sync() {
        draft = t.value
        text = fmt(t.value)
    }

    private func fmt(_ v: Double) -> String {
        var s = String(format: "%.6f", v)
        while s.hasSuffix("0") { s.removeLast() }
        if s.hasSuffix(".") { s.removeLast() }
        return s
    }

    private func save(_ v: Double) async {
        busy = true; errorText = nil
        defer { busy = false }
        do {
            let r = try await apiClient.api.setSimilarityThreshold(t.key, value: v)
            onSaved(r)
            Haptics.success()
        } catch {
            errorText = API.thresholdErrorMessage(error)
            Haptics.error()
        }
    }

    private func reset() async {
        busy = true; errorText = nil
        defer { busy = false }
        do {
            let r = try await apiClient.api.resetSimilarityThreshold(t.key)
            onSaved(r)
            Haptics.success()
        } catch {
            errorText = API.thresholdErrorMessage(error)
            Haptics.error()
        }
    }
}
