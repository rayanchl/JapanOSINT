import SwiftUI
import Combine

struct FollowPanel: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var ws: WebSocketClient
    @Environment(\.theme) private var theme

    @State private var events: [FollowEvent] = []
    @State private var paused = false
    @State private var loading = false
    @State private var primeFailed = false
    /// Why the history prime answered without data, when that is the
    /// server's own answer rather than an outage: the native server has no
    /// collector-tap ring buffer and answers GET /api/follow/recent with
    /// 501 not_implemented on every call (core/miscapi.c miscapi_follow_recent),
    /// and the route is operator-only (403). Either used to read as "backend
    /// offline" with a retry that could never succeed.
    @State private var historyNote: String?
    @State private var subscription: AnyCancellable?

    private var maxEvents: Int { max(1, settings.followLogMaxEntries) }

    var body: some View {
        VStack(spacing: 0) {
            controls
            Divider()
            if let historyNote {
                Text(historyNote)
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
                    .background(theme.surfaceElevated)
            }
            // `primeFailed` is the REST prime failing — that alone is the
            // offline signal. The realtime socket being down is not: it is an
            // enhancement over the same data, and gating on it made this claim
            // the backend was unreachable whenever /ws wasn't serving.
            if events.isEmpty && primeFailed && !loading {
                OfflineStateView(retry: { Task { await primeFromBackend() } })
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Follow log")
        .task { await primeFromBackend(); subscribe() }
        .onDisappear { subscription?.cancel() }
    }

    private var controls: some View {
        HStack(spacing: 8) {
            Spacer()
            Toggle("Pause", isOn: $paused)
                .toggleStyle(.switch)
                .labelsHidden()
                .accessibilityLabel("Pause the live follow log")
            Text("Pause").font(.caption2).foregroundStyle(theme.textMuted)
                .accessibilityHidden(true)
            Button("Clear") { events.removeAll() }
                .buttonStyle(.bordered)
            Button {
                Task { await primeFromBackend() }
            } label: { Image(systemName: "arrow.clockwise") }
                .disabled(loading)
                .accessibilityLabel("Reload follow log")
        }
        .padding(8)
    }

    private var list: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(spacing: 1) {
                    ForEach(events) { ev in
                        row(ev)
                    }
                }
            }
            .onChange(of: events.count) { _, _ in
                if !paused, let last = events.last {
                    withAnimation { proxy.scrollTo(last.id, anchor: .bottom) }
                }
            }
        }
    }

    private func row(_ ev: FollowEvent) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            Text(ev.method ?? "GET")
                .font(.system(.caption2, design: .monospaced).bold())
                .frame(width: 44, alignment: .leading)
                .foregroundStyle(theme.accent)
            Text(statusText(ev.status))
                .font(.system(.caption2, design: .monospaced).bold())
                .frame(width: 36, alignment: .leading)
                .foregroundStyle(statusColor(ev.status))
            VStack(alignment: .leading, spacing: 1) {
                Text(ev.url ?? "—")
                    .font(.system(.caption2, design: .monospaced))
                    .lineLimit(2)
                    .foregroundStyle(theme.text)
                HStack(spacing: 8) {
                    if let c = ev.collector { Text(c).font(.caption2).foregroundStyle(theme.textMuted) }
                    if let n = ev.record_count {
                        Text("\(n) rec").font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                    }
                    if let ms = ev.duration_ms {
                        Text("\(Int(ms))ms").font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                    }
                    if let b = ev.bytes {
                        Text(byteFmt(b)).font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                    }
                }
            }
            Spacer()
        }
        .padding(6)
        .background(theme.surfaceElevated)
        // Collapse the columns into one spoken phrase; otherwise VoiceOver
        // reads "GET", "200", the URL and each metric as separate elements.
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(accessibilityLabel(ev))
    }

    private func accessibilityLabel(_ ev: FollowEvent) -> String {
        var parts: [String] = [
            ev.method ?? "GET",
            "status \(statusText(ev.status))",
            ev.url ?? "no URL"
        ]
        if let c = ev.collector { parts.append("collector \(c)") }
        if let n = ev.record_count { parts.append("\(n) records") }
        if let ms = ev.duration_ms { parts.append("\(Int(ms)) milliseconds") }
        if let b = ev.bytes { parts.append(byteFmt(b)) }
        return parts.joined(separator: ", ")
    }

    private func statusText(_ s: Int?) -> String {
        guard let s else { return "—" }
        return String(s)
    }
    private func statusColor(_ s: Int?) -> Color {
        guard let s else { return theme.textMuted }
        switch s {
        case 200..<300: return theme.success
        case 300..<400: return theme.warning
        case 400..<500: return theme.warning
        case 500...:    return theme.danger
        default:        return theme.accent
        }
    }
    private func byteFmt(_ n: Int) -> String {
        ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
    }

    private func subscribe() {
        subscription = ws.follow.sink { ev in
            Task { @MainActor in
                guard !paused else { return }
                events.append(ev)
                if events.count > maxEvents {
                    events.removeFirst(events.count - maxEvents)
                }
            }
        }
    }

    private func primeFromBackend() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api.recentFollow(limit: maxEvents)
            events = env.events
            primeFailed = false
            historyNote = nil
        } catch APIError.http(501, let body) {
            struct Body: Decodable { let detail: String? }
            let detail = body.data(using: .utf8)
                .flatMap { try? JSONDecoder().decode(Body.self, from: $0) }?.detail
            historyNote = "Request history is not available on this server"
                + (detail.map { " (\($0))" } ?? "")
                + ". Live events appear below while the realtime channel is connected."
            primeFailed = false
        } catch APIError.http(403, _) {
            historyNote = "Request history is a platform-operator view; this account cannot read it."
            primeFailed = false
        } catch {
            primeFailed = true
        }
    }
}
