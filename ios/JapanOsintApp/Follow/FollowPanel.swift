import SwiftUI
import Combine

struct FollowPanel: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var ws: WebSocketClient
    @Environment(\.theme) private var theme

    @State private var events: [FollowEvent] = []
    @State private var paused = false
    @State private var loading = false
    @State private var primeFailed = false
    @State private var subscription: AnyCancellable?

    private var maxEvents: Int { max(1, settings.followLogMaxEntries) }

    var body: some View {
        VStack(spacing: 0) {
            controls
            Divider()
            if events.isEmpty && primeFailed && !ws.isConnected && !loading {
                OfflineStateView(retry: { Task { await primeFromBackend() } })
            } else {
                list
            }
        }
        .background(theme.surface.ignoresSafeArea())
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
            Text("Pause").font(.caption2).foregroundStyle(theme.textMuted)
            Button("Clear") { events.removeAll() }
                .buttonStyle(.bordered)
            Button {
                Task { await primeFromBackend() }
            } label: { Image(systemName: "arrow.clockwise") }
                .disabled(loading)
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
            let env = try await API(baseURL: settings.backendBaseURL).recentFollow(limit: maxEvents)
            events = env.events
            primeFailed = false
        } catch {
            primeFailed = true
        }
    }
}
