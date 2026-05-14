import SwiftUI

struct SettingsTab: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var registry: LayerRegistry
    @EnvironmentObject var saved: SavedStore
    @Environment(\.theme) private var theme
    @Environment(\.scenePhase) private var scenePhase

    @State private var phase: HealthPhase = .idle
    /// Persisted snapshot of the most recent check, kept around so the result
    /// card stays visible after the button label has reverted to "Check
    /// connection". Cleared only when a new check starts.
    @State private var lastResult: ResultSnapshot? = nil

    @State private var confirmClearSaved = false
    @State private var confirmClearLayerCache = false

    /// Per-button transient feedback for the Data & cache section.
    /// .working = spinner (held for at least 1 s), .done = green checkmark
    /// flash for ~1 s, .idle = original red/blue label. Driven by
    /// `runWithFeedback(_:action:)` so all four buttons feel identical.
    @State private var refreshFeedback: ActionFeedback = .idle
    @State private var clearLayerFeedback: ActionFeedback = .idle
    @State private var clearSavedFeedback: ActionFeedback = .idle
    @State private var disableAllFeedback: ActionFeedback = .idle

    /// Face-ID gate. Settings holds the backend URL + clear-data buttons —
    /// keep behind the same biometric wall as the API-keys tab so a borrowed
    /// device can't reconfigure the app. Re-locks on app background so an
    /// unattended phone returning from background re-prompts.
    @State private var unlocked: Bool = false
    @State private var authError: String?
    @State private var authInFlight: Bool = false

    /// Server-restart UI state. Triggered from `serverControlSection` so the
    /// user can force every collector to re-read API keys + other env vars
    /// after editing them in the API Keys tab.
    @State private var restarting: Bool = false
    @State private var restartError: String?
    @State private var confirmRestart: Bool = false

    enum HealthPhase: Equatable {
        case idle
        case checking
        case live(HealthInfo)
        case failure(String)
    }

    enum ResultSnapshot: Equatable {
        case live(HealthInfo)
        case failure(String)
    }

    struct HealthInfo: Equatable {
        let status: String
        let timestamp: Date?
    }

    var body: some View {
        Group {
            if unlocked {
                Form {
                    backendSection
                    appearanceSection
                    liveFeedsSection
                    mapSection
                    networkRefreshSection
                    listsLimitsSection
                    translationSection
                    serverControlSection
                    dataAndCacheSection
                    tipsSection
                }
                .disabled(restarting)
            } else {
                lockedView
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(unlocked ? .automatic : .inline)
        .toolbar {
            if unlocked {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        unlocked = false
                        authError = nil
                    } label: {
                        Image(systemName: "lock.fill")
                    }
                    .accessibilityLabel("Lock Settings")
                }
            }
        }
        .task { if !unlocked { await tryUnlock() } }
        .onChange(of: scenePhase) { _, phase in
            // Re-lock when the app leaves the foreground so an unattended
            // device returning from background re-prompts.
            if phase == .background { unlocked = false }
        }
    }

    // MARK: - Server control

    /// Sits above `dataAndCacheSection` so it's reachable without a long
    /// scroll. The Settings tab is already Face-ID gated, so no extra
    /// biometric prompt — confirmation dialog is enough friction for a
    /// destructive action.
    private var serverControlSection: some View {
        Section {
            Button(role: .destructive) {
                confirmRestart = true
            } label: {
                HStack {
                    Image(systemName: "arrow.clockwise.circle.fill")
                    Text(restarting ? "Restarting…" : "Restart server")
                    Spacer()
                    if restarting { ProgressView().controlSize(.small) }
                }
            }
            .disabled(restarting)
            // Dialog attached to the trigger Button, not the Section. Section-
            // hosted .confirmationDialog renders the body buttons but swallows
            // the `message:` text on iOS 17+; per-button hosting works.
            .confirmationDialog("Restart server?",
                                isPresented: $confirmRestart,
                                titleVisibility: .visible) {
                Button("Restart", role: .destructive) {
                    Haptics.tap(.medium)
                    Task { await doRestart() }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("All in-flight requests will be dropped. The app reconnects automatically once the server is back.")
            }
            if let restartError {
                Text(restartError)
                    .font(.caption)
                    .foregroundStyle(theme.warning)
            }
        } header: {
            Text("Server")
        } footer: {
            Text("Forces every collector to re-read API keys and other env vars. The dev server uses node --watch and respawns automatically; takes 2–4 s to come back.")
                .font(.caption2)
        }
    }

    private func doRestart() async {
        restartError = nil
        restarting = true
        defer { restarting = false }
        do {
            try await API(baseURL: settings.backendBaseURL).restartServer()
        } catch {
            // Connection-reset is expected — the server kills the socket as
            // it tears down. Treat any post-POST failure as "probably
            // restarting" and fall through to the health poll for liveness.
        }
        // Poll /api/health up to 30 s (1 s cadence) until the server is back.
        let deadline = Date().addingTimeInterval(30)
        while Date() < deadline {
            try? await Task.sleep(for: .seconds(1))
            if (try? await API(baseURL: settings.backendBaseURL).health()) != nil {
                Haptics.success()
                return
            }
        }
        restartError = "Server didn't come back within 30 s. Check the host."
        Haptics.error()
    }

    // MARK: - Locked screen

    private var lockedView: some View {
        VStack(spacing: 18) {
            Image(systemName: "lock.fill")
                .font(.system(size: 56, weight: .regular))
                .foregroundStyle(theme.accent)
            Text("Settings Locked")
                .font(.title3.bold())
                .foregroundStyle(theme.text)
            Text("Authenticate to view or change Settings.")
                .font(.subheadline)
                .foregroundStyle(theme.textMuted)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 24)
            Button {
                Task { await tryUnlock() }
            } label: {
                Label(authInFlight ? "Authenticating…" : "Unlock with Face ID",
                      systemImage: "faceid")
                    .frame(maxWidth: 260)
            }
            .buttonStyle(.borderedProminent)
            .disabled(authInFlight)
            if let authError {
                Text(authError)
                    .font(.caption)
                    .foregroundStyle(theme.warning)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 24)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(theme.surface)
    }

    private func tryUnlock() async {
        guard !authInFlight, !unlocked else { return }
        authInFlight = true
        defer { authInFlight = false }
        switch await BiometricAuth.authenticate(reason: "Unlock Settings") {
        case .success:
            await MainActor.run {
                unlocked = true
                authError = nil
            }
        case .failure(let msg):
            await MainActor.run { authError = msg }
        }
    }

    // MARK: - Backend section

    private var backendSection: some View {
        Section("Backend") {
            TextField("URL", text: $settings.backendBaseURL)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)
                .font(.system(.body, design: .monospaced))
                // System theme applies `.fontDesign(.default)` at the root
                // (see JapanOsintApp.swift) which overrides the design of the
                // explicit `.system(_, design: .monospaced)` above. Set the
                // local environment value back to monospaced so this field
                // stays mono regardless of theme.
                .fontDesign(.monospaced)

            checkConnectionButton

            if case .live(let info) = lastResult {
                healthCard(info)
            } else if case .failure(let msg) = lastResult {
                failureCard(msg)
            }
        }
    }

    @ViewBuilder
    private var checkConnectionButton: some View {
        Button(action: tapCheck) {
            HStack(spacing: 8) {
                icon
                Text(label)
                    .fontWeight(.medium)
                Spacer()
            }
            .foregroundStyle(foreground)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 6)
        }
        .disabled(phase == .checking)
        .animation(.easeInOut(duration: 0.35), value: phaseId)
    }

    @ViewBuilder
    private var icon: some View {
        switch phase {
        case .idle:
            Image(systemName: "wifi")
        case .checking:
            ProgressView().scaleEffect(0.8)
        case .live:
            Image(systemName: "checkmark.circle.fill")
                .foregroundStyle(theme.success)
        case .failure:
            Image(systemName: "xmark.circle.fill")
                .foregroundStyle(theme.danger)
        }
    }

    private var label: String {
        switch phase {
        case .idle:     return "Check connection"
        case .checking: return "Checking…"
        case .live:     return "Server live"
        case .failure:  return "Connection failed"
        }
    }

    private var foreground: Color {
        switch phase {
        case .idle, .checking: return theme.text
        case .live:            return theme.success
        case .failure:         return theme.danger
        }
    }

    /// Used as the animation key — collapses associated values so the
    /// transition triggers on phase change but not on data updates.
    private var phaseId: Int {
        switch phase {
        case .idle: return 0
        case .checking: return 1
        case .live: return 2
        case .failure: return 3
        }
    }

    private func healthCard(_ info: HealthInfo) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Health response")
                    .font(.caption.bold())
                    .foregroundStyle(theme.textMuted)
                Spacer()
                Text("HTTP 200")
                    .font(.caption2.bold().monospacedDigit())
                    .foregroundStyle(theme.success)
                    .padding(.horizontal, 6).padding(.vertical, 2)
                    .background(theme.success.opacity(0.15), in: Capsule())
            }

            LabeledContent("Status") {
                Text(info.status)
                    .font(.caption.bold())
                    .foregroundStyle(.white)
                    .padding(.horizontal, 8).padding(.vertical, 3)
                    .background(info.status == "ok" ? theme.success : theme.warning,
                                in: Capsule())
            }

            if let ts = info.timestamp {
                LabeledContent("Timestamp") {
                    VStack(alignment: .trailing, spacing: 1) {
                        Text(ts.formatted(date: .abbreviated, time: .standard))
                            .font(.caption.monospacedDigit())
                        Text(relativeFormatter.localizedString(for: ts, relativeTo: Date()))
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                    }
                }
            }
        }
        .padding(.vertical, 4)
    }

    private func failureCard(_ msg: String) -> some View {
        Text(msg)
            .font(.caption)
            .foregroundStyle(theme.danger)
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    // MARK: - Other sections

    private var appearanceSection: some View {
        Section("Appearance") {
            Picker("Theme", selection: Binding(
                get: { settings.appTheme },
                set: { settings.appTheme = $0 })
            ) {
                ForEach(AppTheme.allCases) { t in
                    Text(t.label).tag(t)
                }
            }
            .pickerStyle(.segmented)
            .sensoryFeedback(.selection, trigger: settings.appTheme)
        }
    }

    private var liveFeedsSection: some View {
        Section {
            Toggle("Show carriages on map", isOn: $settings.liveCarriagesEnabled)
            if settings.liveCarriagesEnabled {
                Toggle("Live trains",  isOn: $settings.liveTrainsEnabled)
                Toggle("Live subways", isOn: $settings.liveSubwaysEnabled)
                Toggle("Live buses",   isOn: $settings.liveBusesEnabled)
            }
        } header: {
            Text("Live feeds")
        } footer: {
            Text("Animated dots for trains, subways and buses on top of their route lines. Planes are always shown when the Flights layer is on. Disable on cellular to save data.")
        }
    }

    private var mapSection: some View {
        Section("Map") {
            Stepper(value: $settings.maxFeaturesPerLayer, in: 100...2000, step: 100) {
                HStack {
                    Text("Max points per layer")
                    Spacer()
                    Text("\(settings.maxFeaturesPerLayer)")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.maxFeaturesPerLayer)
            Stepper(value: $settings.maxLinesPolygonsPerLayer, in: 50...2000, step: 50) {
                HStack {
                    Text("Max lines/polygons per layer")
                    Spacer()
                    Text("\(settings.maxLinesPolygonsPerLayer)")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.maxLinesPolygonsPerLayer)
            Text("Lower if the map lags on dense layers.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
    }

    private var networkRefreshSection: some View {
        Section {
            Stepper(value: $settings.cameraRefreshSeconds, in: 5...120, step: 5) {
                refreshRow(label: "Camera snapshot refresh",
                           value: "\(settings.cameraRefreshSeconds)s")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.cameraRefreshSeconds)
            Stepper(value: $settings.departuresRefreshSeconds, in: 10...120, step: 10) {
                refreshRow(label: "Departures board refresh",
                           value: "\(settings.departuresRefreshSeconds)s")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.departuresRefreshSeconds)
            Stepper(value: $settings.apiDefaultTimeoutSeconds, in: 10...60, step: 5) {
                refreshRow(label: "Default network timeout",
                           value: "\(settings.apiDefaultTimeoutSeconds)s")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.apiDefaultTimeoutSeconds)
        } header: {
            Text("Network & refresh")
        } footer: {
            Text("Longer intervals save data on cellular; shorter feels more live on Wi-Fi.")
        }
    }

    private var listsLimitsSection: some View {
        Section {
            Stepper(value: $settings.followLogMaxEntries, in: 50...500, step: 50) {
                refreshRow(label: "Follow log history",
                           value: "\(settings.followLogMaxEntries)")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.followLogMaxEntries)
            Stepper(value: $settings.dbTablePageSize, in: 25...200, step: 25) {
                refreshRow(label: "Database table page size",
                           value: "\(settings.dbTablePageSize)")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.dbTablePageSize)
            Stepper(value: $settings.departuresShown, in: 5...20, step: 1) {
                refreshRow(label: "Departures shown",
                           value: "\(settings.departuresShown)")
            }
            .sensoryFeedback(.impact(weight: .light), trigger: settings.departuresShown)
        } header: {
            Text("Lists & search limits")
        }
    }

    private var translationSection: some View {
        Section {
            Toggle("Auto-translate search queries",
                   isOn: $settings.autoTranslateSearch)
                .sensoryFeedback(.selection, trigger: settings.autoTranslateSearch)
            Toggle("Show translate button on Japanese text",
                   isOn: $settings.translateButtonEnabled)
                .sensoryFeedback(.selection, trigger: settings.translateButtonEnabled)
            Toggle("Show romaji",
                   isOn: $settings.showRomaji)
                .sensoryFeedback(.selection, trigger: settings.showRomaji)
            Picker("Translate into", selection: Binding(
                get: { settings.translateTargetLanguageRaw },
                set: { settings.translateTargetLanguageRaw = $0 })
            ) {
                Text("Device default").tag("")
                Text("English").tag("en")
                Text("French").tag("fr")
                Text("Spanish").tag("es")
                Text("German").tag("de")
                Text("Korean").tag("ko")
                Text("Chinese (Simplified)").tag("zh-Hans")
                Text("Chinese (Traditional)").tag("zh-Hant")
            }
        } header: {
            Text("Translation")
        } footer: {
            Text("Uses Apple's on-device Translation framework. The first translation may prompt to download a language pack. Auto-translate search runs your query in both English and Japanese and merges the results.")
        }
    }

    private var dataAndCacheSection: some View {
        Section {
            HStack {
                Text("\(registry.layers.count) layers cached")
                    .monospacedDigit()
                Spacer()
                Button {
                    Task {
                        await runWithFeedback($refreshFeedback) {
                            await registry.reload(baseURL: settings.backendBaseURL)
                        }
                    }
                } label: {
                    feedbackLabel(refreshFeedback,
                                  idleText: "Refresh",
                                  idleColor: theme.accent)
                }
                .disabled(refreshFeedback != .idle)
            }

            Button(role: .destructive) {
                confirmClearLayerCache = true
            } label: {
                feedbackLabel(clearLayerFeedback,
                              idleText: "Clear layer cache",
                              idleColor: theme.danger)
            }
            .disabled(clearLayerFeedback != .idle)
            // Per-button host so the message: closure renders. See the
            // serverControlSection note for the SwiftUI quirk this avoids.
            .confirmationDialog("Clear cached layer catalogue?",
                                isPresented: $confirmClearLayerCache,
                                titleVisibility: .visible) {
                Button("Clear and refetch", role: .destructive) {
                    Task {
                        await runWithFeedback($clearLayerFeedback) {
                            settings.clearLayerRegistryCache()
                            await registry.reload(baseURL: settings.backendBaseURL)
                        }
                    }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("The cached layer list is wiped and re-fetched from the backend.")
            }

            Button(role: .destructive) {
                confirmClearSaved = true
            } label: {
                feedbackLabel(clearSavedFeedback,
                              idleText: "Clear saved items (\(saved.items.count))",
                              idleColor: theme.danger)
            }
            .disabled(clearSavedFeedback != .idle)
            .confirmationDialog("Clear saved items?",
                                isPresented: $confirmClearSaved,
                                titleVisibility: .visible) {
                Button("Clear \(saved.items.count) items", role: .destructive) {
                    Task {
                        await runWithFeedback($clearSavedFeedback) {
                            saved.clearAll()
                        }
                    }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("This removes all bookmarked features from this device.")
            }

            Button(role: .destructive) {
                Task {
                    await runWithFeedback($disableAllFeedback) {
                        settings.disableAll()
                    }
                }
            } label: {
                feedbackLabel(disableAllFeedback,
                              idleText: "Disable all layers",
                              idleColor: theme.danger)
            }
            .disabled(disableAllFeedback != .idle)
        } header: {
            Text("Data & cache")
        }
    }

    // MARK: - Unified click feedback

    private enum ActionFeedback: Equatable { case idle, working, done }

    /// Runs `action`, holds the spinner for at least `minSpinnerMs`, flashes
    /// "Done" for `doneDurationMs`, then reverts to .idle. Single source of
    /// truth for the timing UX so all four buttons in dataAndCacheSection
    /// feel identical even when the underlying action is instant (sync).
    private func runWithFeedback(
        _ binding: Binding<ActionFeedback>,
        minSpinnerMs: Int = 1000,
        doneDurationMs: Int = 1000,
        action: @escaping () async -> Void
    ) async {
        Haptics.tap()
        binding.wrappedValue = .working
        let start = Date()
        await action()
        let elapsedMs = Int(Date().timeIntervalSince(start) * 1000)
        if elapsedMs < minSpinnerMs {
            try? await Task.sleep(for: .milliseconds(minSpinnerMs - elapsedMs))
        }
        binding.wrappedValue = .done
        Haptics.success()
        try? await Task.sleep(for: .milliseconds(doneDurationMs))
        binding.wrappedValue = .idle
    }

    @ViewBuilder
    private func feedbackLabel(
        _ state: ActionFeedback,
        idleText: String,
        idleColor: Color
    ) -> some View {
        switch state {
        case .idle:
            Text(idleText).foregroundStyle(idleColor)
        case .working:
            HStack(spacing: 8) {
                ProgressView().controlSize(.small)
                Text(idleText).foregroundStyle(idleColor.opacity(0.55))
            }
        case .done:
            HStack(spacing: 6) {
                Image(systemName: "checkmark.circle.fill")
                Text("Done")
            }
            .foregroundStyle(theme.success)
        }
    }

    private func refreshRow(label: String, value: String) -> some View {
        HStack {
            Text(label)
            Spacer()
            Text(value)
                .font(.caption.monospacedDigit())
                .foregroundStyle(theme.textMuted)
        }
    }

    private var tipsSection: some View {
        Section("Tips") {
            VStack(alignment: .leading, spacing: 4) {
                HStack(spacing: 4) {
                    Text("Simulator use:")
                    Text("http://127.0.0.1:4000")
                        .font(.caption.monospaced())
                        .fontDesign(.monospaced)
                        .foregroundStyle(theme.accent)
                        .textSelection(.enabled)
                }
                HStack(spacing: 4) {
                    Text("Local network use:")
                    Text("http://192.168.1.X:4000")
                        .font(.caption.monospaced())
                        .fontDesign(.monospaced)
                        .foregroundStyle(theme.accent)
                        .textSelection(.enabled)
                }
            }
            .font(.caption)
        }
    }

    // MARK: - Health check

    private func tapCheck() {
        Task { await runCheck() }
    }

    private func runCheck() async {
        withAnimation { phase = .checking }
        // Hold the "Checking…" state for at least 1s so the user can see
        // the spinner even if the backend responds instantly (localhost dev).
        let started = ContinuousClock.now
        let minDuration: Duration = .seconds(1)
        do {
            let data = try await API(baseURL: settings.backendBaseURL).health()
            let info = parseHealth(data: data)
            let remaining = minDuration - (ContinuousClock.now - started)
            if remaining > .zero { try? await Task.sleep(for: remaining) }
            withAnimation {
                phase = .live(info)
                lastResult = .live(info)
            }
            Haptics.success()
            try? await Task.sleep(for: .seconds(5))
            // Reset the button to "Check connection" while leaving the
            // result card up. Only flip if no newer check has overwritten us.
            if case .live(let current) = phase, current == info {
                withAnimation(.easeOut(duration: 0.5)) { phase = .idle }
            }
        } catch {
            let msg = error.localizedDescription
            let remaining = minDuration - (ContinuousClock.now - started)
            if remaining > .zero { try? await Task.sleep(for: remaining) }
            withAnimation {
                phase = .failure(msg)
                lastResult = .failure(msg)
            }
            Haptics.error()
            try? await Task.sleep(for: .seconds(5))
            if case .failure(let current) = phase, current == msg {
                withAnimation(.easeOut(duration: 0.5)) { phase = .idle }
            }
        }
    }

    private func parseHealth(data: Data) -> HealthInfo {
        struct Body: Decodable { let status: String?; let timestamp: String? }
        let body = (try? JSONDecoder().decode(Body.self, from: data)) ?? Body(status: nil, timestamp: nil)
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        let date = body.timestamp.flatMap {
            f.date(from: $0) ?? ISO8601DateFormatter().date(from: $0)
        }
        return HealthInfo(
            status: body.status ?? "unknown",
            timestamp: date
        )
    }

    private var relativeFormatter: RelativeDateTimeFormatter {
        let f = RelativeDateTimeFormatter()
        f.unitsStyle = .abbreviated
        return f
    }
}
