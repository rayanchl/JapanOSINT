import SwiftUI

/// Two-button row for "Ping now" + "Allow scheduled probing" used both by
/// the source-detail card (Sources tab) and the API key detail's "Used by"
/// rows (API Keys tab). Both actions are Face-ID-fenced via `BiometricAuth`
/// because they hit external endpoints and may flip server-side opt-in
/// state.
struct ProbeActionsView: View {
    let row: StatusRow
    var onUpdate: (StatusRow) -> Void

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var pinging = false
    @State private var consentBusy = false
    @State private var actionError: String?

    var body: some View {
        let configured = row.configured == true
        let consented = row.probeConsent == true
        VStack(spacing: 6) {
            HStack(spacing: 8) {
                Spacer(minLength: 0)
                Button(action: pingNow) {
                    HStack(spacing: 6) {
                        probeIcon(busy: pinging, systemName: "bolt.fill")
                        Text(pinging ? "Pinging…" : "Ping now")
                            .lineLimit(1)
                    }
                    .frame(height: 18)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .disabled(pinging || !configured)

                Button(action: toggleConsent) {
                    HStack(spacing: 6) {
                        probeIcon(busy: consentBusy,
                                  systemName: consented ? "checkmark.shield.fill" : "shield.slash")
                        Text(consented ? "Scheduled probing on" : "Allow scheduled probing")
                            .lineLimit(1)
                    }
                    .frame(height: 18)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .tint(consented ? theme.success : theme.accent)
                .disabled(consentBusy || (!consented && !configured))
                Spacer(minLength: 0)
            }
            if !configured {
                Text("Set \((row.missingVars ?? []).joined(separator: ", ")) in the API Keys tab first.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .multilineTextAlignment(.center)
            }
            if let err = actionError {
                Text(err)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .multilineTextAlignment(.center)
            }
        }
    }

    /// Fixed-size leading glyph so the button's intrinsic height stays
    /// stable when toggling between the static icon and the spinner — a
    /// raw `ProgressView` measures a hair shorter than `Image(systemName:)`
    /// at the same control size, which makes the two buttons jitter
    /// vertically as ping state flips.
    @ViewBuilder
    private func probeIcon(busy: Bool, systemName: String) -> some View {
        Group {
            if busy {
                ProgressView().controlSize(.mini)
            } else {
                Image(systemName: systemName)
            }
        }
        .frame(width: 14, height: 14)
    }

    private func pingNow() {
        actionError = nil
        pinging = true
        Task {
            defer { Task { @MainActor in pinging = false } }
            switch await BiometricAuth.authenticate(reason: "Ping \(row.name ?? row.id) now") {
            case .failure(let msg):
                await MainActor.run { actionError = msg }
                return
            case .success:
                break
            }
            do {
                let updated = try await API(baseURL: settings.backendBaseURL).probeSource(row.id)
                await MainActor.run { onUpdate(updated) }
            } catch {
                await MainActor.run { actionError = error.localizedDescription }
            }
        }
    }

    private func toggleConsent() {
        actionError = nil
        consentBusy = true
        let next = !(row.probeConsent == true)
        let reason = next
            ? "Enable scheduled probing for \(row.name ?? row.id)"
            : "Disable scheduled probing for \(row.name ?? row.id)"
        Task {
            defer { Task { @MainActor in consentBusy = false } }
            switch await BiometricAuth.authenticate(reason: reason) {
            case .failure(let msg):
                await MainActor.run { actionError = msg }
                return
            case .success:
                break
            }
            do {
                let updated = try await API(baseURL: settings.backendBaseURL)
                    .setProbeConsent(row.id, allow: next)
                await MainActor.run { onUpdate(updated) }
            } catch {
                await MainActor.run { actionError = error.localizedDescription }
            }
        }
    }
}
