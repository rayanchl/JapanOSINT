import SwiftUI

/// Level-1 row: one source. Icon + name + (category · last-fetched · count)
/// + a leading-edge Run button that fires the collector on demand.
struct IntelSourceRow: View {
    let source: IntelSource
    /// Fires after a successful manual Run so the parent can refresh.
    var onRunComplete: (() -> Void)? = nil

    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    @State private var running = false
    @State private var runError: String?

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: registry.symbol(for: source.id))
                .font(.title3)
                .foregroundStyle(registry.color(for: source.id))
                .frame(width: 28, height: 28)

            VStack(alignment: .leading, spacing: 2) {
                Text(source.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                HStack(spacing: 6) {
                    if let cat = source.category {
                        Text(cat).font(.caption2).foregroundStyle(theme.textMuted)
                        Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    }
                    Text(relativeTime(source.last_fetched ?? source.last_published))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                    Text("\(source.item_count) items")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .monospacedDigit()
                }
                if let runError {
                    Text(runError)
                        .font(.caption2)
                        .foregroundStyle(theme.danger)
                        .lineLimit(1)
                }
            }

            Spacer(minLength: 4)

            // Run button — independent hit-target so tapping it does NOT
            // trigger the parent NavigationLink's row-tap.
            Button {
                Task { await runNow() }
            } label: {
                if running {
                    ProgressView().controlSize(.mini)
                } else {
                    Image(systemName: "play.fill")
                        .font(.subheadline.weight(.bold))
                        .foregroundStyle(theme.accent)
                        .frame(width: 32, height: 32)
                        .background(theme.accent.opacity(0.12), in: Circle())
                }
            }
            .buttonStyle(.plain)
            .disabled(running)
            .accessibilityLabel(running ? "Running…" : "Run \(source.name)")
        }
        .padding(.vertical, 2)
    }

    private func runNow() async {
        running = true
        runError = nil
        defer { running = false }
        do {
            _ = try await API(baseURL: settings.backendBaseURL).intelRunSource(source.id)
            onRunComplete?()
        } catch {
            runError = error.localizedDescription
        }
    }
}

func relativeTime(_ iso: String?) -> String {
    guard let iso, let date = isoToDate(iso) else { return "—" }
    let f = RelativeDateTimeFormatter()
    f.unitsStyle = .abbreviated
    return f.localizedString(for: date, relativeTo: Date())
}

private func isoToDate(_ s: String) -> Date? {
    let f1 = ISO8601DateFormatter()
    f1.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    if let d = f1.date(from: s) { return d }
    let f2 = ISO8601DateFormatter()
    f2.formatOptions = [.withInternetDateTime]
    return f2.date(from: s)
}
