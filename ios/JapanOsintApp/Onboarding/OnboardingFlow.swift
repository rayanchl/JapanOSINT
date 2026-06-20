import SwiftUI
import Combine

/// First-run wizard. Shown by `RootView` until `AuthSession.gate == .ready`.
///
/// Steps: connect (backend + Supabase config) → sign in/up → app-power
/// showcase → layer-exploitation showcase → confirm workspace → API-keys
/// nudge → first collector run (with layer picker) → finish.
struct OnboardingFlow: View {
    @EnvironmentObject private var settings: AppSettings
    @EnvironmentObject private var auth: AuthSession
    @Environment(\.theme) private var theme

    enum Step: Int, CaseIterable {
        case connect, auth, introPower, introLayers, workspace, keys, firstRun, finish
    }

    @State private var step: Step = .connect

    /// Animated step change — drives the classic right-to-left card slide.
    private func go(_ next: Step) {
        withAnimation(.easeInOut(duration: 0.35)) { step = next }
    }

    var body: some View {
        ZStack {
            theme.surface.ignoresSafeArea()
            VStack(spacing: 0) {
                progressBar
                GeometryReader { geo in
                    ScrollView {
                        VStack(alignment: .leading, spacing: 20) {
                            stepContent
                        }
                        .padding(24)
                        // Keep the onboarding card a readable width on the wide
                        // Mac window (no-op on iOS, where it fills the screen).
                        .compatReadableWidth(620)
                        .frame(maxWidth: .infinity, minHeight: geo.size.height,
                                alignment: .top)
                    }
                    .id(step)
                    .transition(.asymmetric(
                        insertion: .move(edge: .trailing).combined(with: .opacity),
                        removal: .move(edge: .leading).combined(with: .opacity)
                    ))
                    .clipped()
                }
            }
            .overlay(alignment: .bottom) { floatingContinue }
            .animation(.spring(response: 0.35, dampingFraction: 0.8),
                       value: settings.activeLayerIds.isEmpty)
            .animation(.easeInOut(duration: 0.35), value: step)
        }
        .tint(theme.accent)
        .preferredColorScheme(settings.appTheme.colorScheme)
    }

    @ViewBuilder
    private var stepContent: some View {
        Group {
            switch step {
            case .connect:  ConnectStep(advance: { go(.auth) })
            case .auth:     AuthStep(advance: { go(.introPower) })
            case .introPower:  IntroPowerStep(advance: { go(.introLayers) })
            case .introLayers: IntroLayersStep(advance: { go(.workspace) })
            case .workspace: WorkspaceStep(advance: { go(.keys) })
            case .keys:     KeysNudgeStep(advance: { go(.firstRun) })
            case .firstRun: FirstRunStep(advance: { go(.finish) })
            case .finish:   FinishStep(done: { auth.completeOnboarding() })
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// Sticky CTA that hovers over the lower part of the layer-picker so the
    /// user can proceed without scrolling to the bottom of the long list.
    @ViewBuilder
    private var floatingContinue: some View {
        if step == .firstRun, !settings.activeLayerIds.isEmpty {
            let n = settings.activeLayerIds.count
            Button {
                go(.finish)
            } label: {
                Text("Continue with \(n) layer\(n == 1 ? "" : "s")")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
            }
            .buttonStyle(.borderedProminent)
            .tint(theme.warning)
            .controlSize(.large)
            .padding(.horizontal, 24)
            .padding(.bottom, 12)
            .shadow(color: .black.opacity(0.25), radius: 10, y: 4)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }

    private var progressBar: some View {
        HStack(spacing: 6) {
            ForEach(Step.allCases, id: \.rawValue) { s in
                Capsule()
                    .fill(s.rawValue <= step.rawValue ? theme.accent : theme.textMuted.opacity(0.3))
                    .frame(height: 4)
            }
        }
        .padding(.horizontal, 24)
        .padding(.top, 16)
    }
}

// MARK: - Step 1: welcome / feature intro
//
// Backend + Supabase config is now baked in (see `BuildConfig`), so the
// first page no longer collects anything — it introduces the product with
// an auto-sliding simulation of the three core surfaces (Map · Alerts ·
// Intel). Self-host users can still repoint the backend later under
// Console → Settings.

private struct ConnectStep: View {
    @Environment(\.theme) private var theme
    let advance: () -> Void

    /// Currently shown showcase panel; auto-advances and is swipeable.
    @State private var panel = 0
    private let advanceTimer = Timer
        .publish(every: 6.4, on: .main, in: .common)
        .autoconnect()

    private let panels: [(title: String, subtitle: String)] = [
        ("Tactical map",
         "Flights, ships, trains, cameras and cyber threats fused onto one live surface."),
        ("Alerts that fire",
         "Write a rule once; get pinged the instant new intel matches it."),
        ("Unified intel feed",
         "Every collector's findings, searchable and translated, in one stream."),
    ]

    var body: some View {
        Group {
            Text("OSINTWorks").font(.largeTitle.bold())
            Text("Real-time open-source intelligence, fused onto one tactical surface.")
                .foregroundStyle(theme.textMuted)

            TabView(selection: $panel) {
                showcasePanel(0) { AnimatedMapMock() }
                showcasePanel(1) { AnimatedAlertsMock() }
                showcasePanel(2) { AnimatedIntelMock() }
            }
            .compatPageTabViewStyle()
            .frame(height: 360)
            .onReceive(advanceTimer) { _ in
                withAnimation(.easeInOut(duration: 0.5)) {
                    panel = (panel + 1) % panels.count
                }
            }

            Button("Get started") { advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }

    @ViewBuilder
    private func showcasePanel<Mock: View>(_ index: Int,
                                           @ViewBuilder mock: () -> Mock) -> some View {
        VStack(spacing: 14) {
            mock()
                .frame(height: 230)
                .frame(maxWidth: .infinity)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 18))
            VStack(spacing: 4) {
                Text(panels[index].title).font(.headline)
                Text(panels[index].subtitle)
                    .font(.footnote)
                    .foregroundStyle(theme.textMuted)
                    .multilineTextAlignment(.center)
            }
            .padding(.horizontal, 8)
        }
        .padding(.bottom, 56)   // push the caption up so it clears the page dots
        .tag(index)
    }
}

// MARK: - Step 2: auth

private struct AuthStep: View {
    @EnvironmentObject private var auth: AuthSession
    @Environment(\.theme) private var theme
    let advance: () -> Void

    @State private var email = ""
    @State private var password = ""
    @State private var isSignUp = false
    @State private var busy = false
    @State private var error: String?

    var body: some View {
        Group {
            Text(isSignUp ? "Create account" : "Sign in").font(.largeTitle.bold())
            Text("Authenticate with your Supabase account. The server provisions a personal workspace on first sign-in.")
                .foregroundStyle(theme.textMuted)

            TextField("Email", text: $email)
                .textContentType(.emailAddress)
                .compatKeyboard(email: true)
                .compatNoAutocap()
                .autocorrectionDisabled()
                .padding(10).background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 8))

            SecureField("Password", text: $password)
                .textContentType(isSignUp ? .newPassword : .password)
                .padding(10).background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 8))

            if let error {
                Text(error).foregroundStyle(theme.danger).font(.footnote)
            }

            Button {
                Task { await submit() }
            } label: {
                HStack {
                    if busy { ProgressView().tint(.black) }
                    Text(busy ? "Working…" : (isSignUp ? "Create account" : "Sign in"))
                }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(busy || email.isEmpty || password.isEmpty)

            Button(isSignUp ? "Have an account? Sign in"
                            : "Need an account? Create one") {
                isSignUp.toggle(); error = nil
            }
            .font(.footnote)
            .frame(maxWidth: .infinity)

            HStack {
                Rectangle().fill(theme.textMuted.opacity(0.3)).frame(height: 1)
                Text("or continue with").font(.caption2)
                    .foregroundStyle(theme.textMuted).fixedSize()
                Rectangle().fill(theme.textMuted.opacity(0.3)).frame(height: 1)
            }
            .padding(.vertical, 4)

            ForEach(OAuthProvider.allCases) { provider in
                Button {
                    Task { await submitProvider(provider) }
                } label: {
                    HStack(spacing: 10) {
                        Group {
                            if provider.usesSFSymbol {
                                Image(systemName: provider.logoName)
                                    .font(.system(size: 18, weight: .bold))
                                    .foregroundStyle(provider.brandForeground)
                            } else {
                                Image(provider.logoName)
                                    .resizable()
                                    .scaledToFit()
                            }
                        }
                        .frame(width: 20, height: 20)
                        Text("Continue with \(provider.label)")
                            .font(.system(size: 16, weight: .semibold))
                            .foregroundStyle(provider.brandForeground)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 12)
                    .background(provider.brandBackground)
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(Color.black.opacity(
                                provider.brandNeedsBorder ? 0.15 : 0), lineWidth: 1)
                    )
                }
                .buttonStyle(.plain)
                .disabled(busy)
                .opacity(busy ? 0.6 : 1)
            }
        }
    }

    private func submit() async {
        busy = true; error = nil
        defer { busy = false }
        do {
            if isSignUp { try await auth.signUp(email: email, password: password) }
            else { try await auth.signIn(email: email, password: password) }
            advance()
        } catch {
            self.error = error.localizedDescription
        }
    }

    private func submitProvider(_ provider: OAuthProvider) async {
        busy = true; error = nil
        defer { busy = false }
        do {
            try await auth.signIn(provider: provider)
            advance()
        } catch {
            self.error = error.localizedDescription
        }
    }
}

// MARK: - Step 3a: app-power showcase

private struct IntroPowerStep: View {
    @Environment(\.theme) private var theme
    let advance: () -> Void

    var body: some View {
        Group {
            Text("Japan, fully mapped").font(.largeTitle.bold())
            Text("Live OSINT from 100+ collectors — flights, ships, trains, cameras, cyber threats and more — fused onto one tactical map that updates in real time.")
                .foregroundStyle(theme.textMuted)

            AnimatedMapMock()
                .frame(height: 230)
                .frame(maxWidth: .infinity)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 18))

            VStack(alignment: .leading, spacing: 14) {
                powerRow("dot.radiowaves.up.forward", "Live feeds",
                         "Vehicle positions and intel stream in continuously.")
                powerRow("clock.arrow.circlepath", "Time machine",
                         "Scrub the slider to replay any moment of history.")
                powerRow("bell.badge.fill", "Alerts",
                         "Get notified the instant a rule matches.")
            }
            .padding(.top, 4)

            Button("Next") { advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }

    @ViewBuilder
    private func powerRow(_ icon: String, _ title: String, _ subtitle: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: icon)
                .font(.title3)
                .foregroundStyle(theme.accent)
                .frame(width: 28)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.headline)
                Text(subtitle).font(.footnote).foregroundStyle(theme.textMuted)
            }
        }
    }
}

/// Self-contained faux map: faint grid, a slow radar sweep, and a handful
/// of OSINT pins that pulse out concentric rings — purely decorative.
private struct AnimatedMapMock: View {
    @Environment(\.theme) private var theme
    @Environment(\.colorScheme) private var scheme

    private let pins: [(CGPoint, String)] = [
        (CGPoint(x: 0.24, y: 0.30), "airplane"),
        (CGPoint(x: 0.64, y: 0.22), "ferry"),
        (CGPoint(x: 0.46, y: 0.55), "tram.fill"),
        (CGPoint(x: 0.79, y: 0.64), "video.fill"),
        (CGPoint(x: 0.33, y: 0.76), "shield.lefthalf.filled"),
    ]

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            GeometryReader { geo in
                let w = geo.size.width, h = geo.size.height
                ZStack {
                    // Radar sweep sits at the back…
                    let base = (t * 45).truncatingRemainder(dividingBy: 360)
                    AngularGradient(
                        gradient: Gradient(stops: [
                            .init(color: theme.accent, location: 0.0),
                            .init(color: theme.accent.opacity(0.0), location: 1.0),
                        ]),
                        center: .center,
                        angle: .degrees(base)
                    )

                    // …and the grid is drawn in front of it. White in light
                    // mode, faint accent in dark mode.
                    Path { p in
                        let step: CGFloat = 28
                        for x in stride(from: 0, through: w, by: step) {
                            p.move(to: CGPoint(x: x, y: 0))
                            p.addLine(to: CGPoint(x: x, y: h))
                        }
                        for y in stride(from: 0, through: h, by: step) {
                            p.move(to: CGPoint(x: 0, y: y))
                            p.addLine(to: CGPoint(x: w, y: y))
                        }
                    }
                    .stroke(scheme == .light ? Color.white.opacity(0.85)
                                             : theme.accent.opacity(0.12),
                            lineWidth: 1)

                    ForEach(Array(pins.enumerated()), id: \.offset) { i, item in
                        // Sawtooth 0→1 ramp: ring expands outward and fades,
                        // then snaps back to start (never contracts).
                        let phase = (t * 0.8 + Double(i) * 0.5)
                            .truncatingRemainder(dividingBy: 1)
                        let ring = 16 + phase * 40
                        let pos = CGPoint(x: item.0.x * w, y: item.0.y * h)
                        ZStack {
                            Circle()
                                .stroke(theme.accent.opacity((1 - phase) * 0.55),
                                        lineWidth: 1.5)
                                .frame(width: ring, height: ring)
                            Image(systemName: item.1)
                                .font(.system(size: 12, weight: .bold))
                                .foregroundStyle(theme.accent)
                                .padding(6)
                                .background(theme.surface)
                                .clipShape(Circle())
                        }
                        .position(pos)
                    }
                }
            }
        }
    }
}

/// Faux alerts console: rule chips at the top, then alert cards that slide
/// up and flash a severity dot as if rules are firing live. Decorative.
private struct AnimatedAlertsMock: View {
    @Environment(\.theme) private var theme

    private let alerts: [(String, String, Color)] = [
        ("Port of Yokohama", "vessel entered geofence",      .orange),
        ("Tokyo ASN 2497",   "new exposed RDP host",          .red),
        ("Narita TFR",       "aircraft squawk 7700",          .yellow),
        ("Osaka cam grid",   "feed back online",              .green),
    ]

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 6) {
                    Image(systemName: "bell.badge.fill")
                        .foregroundStyle(theme.accent)
                    Text("ALERT RULES")
                        .font(.caption.bold())
                        .foregroundStyle(theme.textMuted)
                    Spacer()
                    Text("LIVE")
                        .font(.caption2.bold())
                        .foregroundStyle(theme.accent)
                }

                ForEach(Array(alerts.enumerated()), id: \.offset) { i, a in
                    // Each card has its own 4s cycle, staggered, so they
                    // appear to fire one after another.
                    let phase = (t * 0.25 + Double(i) * 0.25)
                        .truncatingRemainder(dividingBy: 1)
                    let appear = min(1, phase / 0.18)          // slide/fade in
                    let flash  = max(0, 1 - (phase - 0.18) / 0.4)  // dot pulse
                    HStack(spacing: 10) {
                        Circle()
                            .fill(a.2)
                            .frame(width: 9, height: 9)
                            .shadow(color: a.2.opacity(flash),
                                    radius: 5 * flash)
                        VStack(alignment: .leading, spacing: 1) {
                            Text(a.0).font(.caption.bold())
                            Text(a.1).font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                    .padding(8)
                    .background(theme.surface)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
                    .opacity(appear)
                    .offset(y: (1 - appear) * 8)
                }
            }
            .padding(14)
        }
    }
}

/// Faux intel feed: a column of source-tagged rows that scrolls upward in a
/// seamless loop, like collectors streaming findings in. Decorative.
private struct AnimatedIntelMock: View {
    @Environment(\.theme) private var theme

    private let rows: [(String, String)] = [
        ("SHODAN",   "Exposed Modbus on 203.0.x.x"),
        ("JMA",      "M4.2 offshore Ibaraki"),
        ("OPENSKY",  "Mil callsign over Tsushima"),
        ("URLHAUS",  "New phishing kit, .jp TLD"),
        ("AIS",      "Tanker dark since 03:14Z"),
        ("GITHUB",   "Leaked key in jp-fintech repo"),
    ]

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            let rowH: CGFloat = 40
            let loop = CGFloat(rows.count) * rowH
            let offset = CGFloat(t * 16).truncatingRemainder(dividingBy: loop)
            VStack(spacing: 0) {
                // Duplicate the list so the upward scroll never shows a gap.
                ForEach(0..<(rows.count * 2), id: \.self) { idx in
                    let r = rows[idx % rows.count]
                    HStack(spacing: 10) {
                        Text(r.0)
                            .font(.caption2.bold())
                            .foregroundStyle(theme.accent)
                            .frame(width: 64, alignment: .leading)
                        Text(r.1)
                            .font(.caption)
                            .lineLimit(1)
                        Spacer()
                    }
                    .frame(height: rowH)
                    .padding(.horizontal, 14)
                    Divider().overlay(theme.textMuted.opacity(0.15))
                }
            }
            .offset(y: -offset)
            .frame(height: 230, alignment: .top)
            .clipped()
            .mask(LinearGradient(
                colors: [.clear, .black, .black, .clear],
                startPoint: .top, endPoint: .bottom))
        }
    }
}

// MARK: - Step 3b: layer-exploitation showcase

private struct IntroLayersStep: View {
    @EnvironmentObject private var registry: LayerRegistry
    @Environment(\.theme) private var theme
    let advance: () -> Void

    private let showcase = [
        "unified-flights", "unified-ais-ships", "unified-trains", "cameras",
        "earthquake", "wifi-networks", "satellite-imagery", "phishing-feeds-jp",
        "poc-in-github", "onsen-map", "real-estate", "population-density",
        "twitter-geo", "vending-machines", "tabelog-restaurants", "twitch-jp-streams",
    ]

    @State private var highlight = 0
    private let timer = Timer.publish(every: 0.8, on: .main, in: .common).autoconnect()
    private let cols = Array(repeating: GridItem(.flexible(), spacing: 12), count: 4)

    var body: some View {
        Group {
            Text("Exploit every layer").font(.largeTitle.bold())
            Text("Stack and cross-reference 100+ intelligence layers. Toggle any on, fuse the signals, and the map turns raw OSINT into answers.")
                .foregroundStyle(theme.textMuted)

            LazyVGrid(columns: cols, spacing: 14) {
                ForEach(Array(showcase.enumerated()), id: \.offset) { i, id in
                    let on = i == highlight
                    let tint = registry.color(for: id)
                    VStack(spacing: 6) {
                        Image(systemName: registry.symbol(for: id))
                            .font(.system(size: 20, weight: .semibold))
                            .foregroundStyle(on ? Color.black : tint)
                            .frame(width: 48, height: 48)
                            .background(on ? tint : theme.surfaceElevated)
                            .clipShape(RoundedRectangle(cornerRadius: 12))
                            .scaleEffect(on ? 1.14 : 1)
                            .shadow(color: on ? tint.opacity(0.6) : .clear, radius: 8)
                        Text(LayerRegistry.displayName(forId: id))
                            .font(.system(size: 9))
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                }
            }
            .animation(.spring(response: 0.4, dampingFraction: 0.7), value: highlight)
            .onReceive(timer) { _ in
                highlight = (highlight + 1) % showcase.count
            }
            .padding(.vertical, 8)

            Button("Next") { advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }
}

// MARK: - Step 3: workspace

private struct WorkspaceStep: View {
    @EnvironmentObject private var auth: AuthSession
    @Environment(\.theme) private var theme
    let advance: () -> Void

    var body: some View {
        Group {
            Text("Workspace").font(.largeTitle.bold())
            Text("Everyone you invite shares one workspace. Analysts, admins and developers all read the same collected data — the role only scopes what each can change.")
                .foregroundStyle(theme.textMuted)

            RolesAccessDiagram()
                .frame(height: 215)
                .frame(maxWidth: .infinity)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 18))

            if let t = auth.me?.tenant {
                VStack(alignment: .leading, spacing: 8) {
                    Image(systemName: "square.stack.3d.up.fill")
                        .font(.title2)
                        .foregroundStyle(theme.accent)
                    if let email = auth.accountEmail {
                        Text(email).font(.callout)
                    }
                    Text(t.slug)
                        .font(.callout.monospaced())
                        .foregroundStyle(theme.textMuted)
                    if let role = t.role {
                        Text(role)
                            .font(.callout.monospaced())
                            .foregroundStyle(theme.textMuted)
                    }
                }
                .padding(16)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 12))
            }

            if auth.memberships.count > 1 {
                Text("Switch active workspace").font(.headline)
                ForEach(auth.memberships) { m in
                    Button {
                        Task { await auth.switchTenant(m.id) }
                    } label: {
                        HStack {
                            Text(m.name)
                            Spacer()
                            if m.id == auth.me?.tenant?.id {
                                Image(systemName: "checkmark.circle.fill")
                                    .foregroundStyle(theme.accent)
                            }
                        }
                    }
                    .buttonStyle(.bordered)
                }
            }

            Button("Continue") { advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }
}

/// Three roles, one shared dataset. Lines stream from the active role into
/// the central workspace node to show they all hit the same data.
private struct RolesAccessDiagram: View {
    @Environment(\.theme) private var theme

    private let roles: [(String, String)] = [
        ("Analyst",   "magnifyingglass"),
        ("Admin",     "person.badge.key.fill"),
        ("Developer", "chevron.left.forwardslash.chevron.right"),
    ]

    @State private var active = 0
    private let timer = Timer.publish(every: 1.1, on: .main, in: .common)
        .autoconnect()

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            GeometryReader { geo in
                let w = geo.size.width, h = geo.size.height
                let inset: CGFloat = 56
                let roleY = h * 0.20
                let xRole = [inset, w / 2, w - inset]
                let centerX = w / 2
                let centerY = h * 0.74
                let roleBottomY = roleY + 30
                let busY = h * 0.46
                let nodeTopY = centerY - 30

                ZStack {
                    // 90° elbow connectors: down → across → down into node.
                    ForEach(0..<roles.count, id: \.self) { i in
                        let on = i == active
                        Path { p in
                            p.move(to: CGPoint(x: xRole[i], y: roleBottomY))
                            p.addLine(to: CGPoint(x: xRole[i], y: busY))
                            p.addLine(to: CGPoint(x: centerX, y: busY))
                            p.addLine(to: CGPoint(x: centerX, y: nodeTopY))
                        }
                        .stroke(
                            on ? theme.accent : theme.textMuted.opacity(0.30),
                            style: StrokeStyle(
                                lineWidth: on ? 2 : 1, dash: [5, 5],
                                dashPhase: -t.truncatingRemainder(dividingBy: 1) * 20)
                        )
                    }

                    ForEach(0..<roles.count, id: \.self) { i in
                        let on = i == active
                        VStack(spacing: 4) {
                            Image(systemName: roles[i].1)
                                .font(.system(size: 15, weight: .bold))
                                .foregroundStyle(on ? Color.black : theme.accent)
                                .frame(width: 36, height: 36)
                                .background(on ? theme.accent : theme.surface)
                                .clipShape(Circle())
                                .overlay(Circle().stroke(
                                    theme.accent.opacity(on ? 1 : 0.3), lineWidth: 1))
                            Text(roles[i].0).font(.caption2.bold())
                                .foregroundStyle(on ? theme.text : theme.textMuted)
                        }
                        .scaleEffect(on ? 1.1 : 1)
                        .position(x: xRole[i], y: roleY)
                    }

                    VStack(spacing: 3) {
                        Image(systemName: "square.stack.3d.up.fill")
                            .font(.system(size: 22, weight: .bold))
                            .foregroundStyle(theme.accent)
                        Text("One workspace").font(.caption.bold())
                        Text("same data · role-scoped")
                            .font(.caption2).foregroundStyle(theme.textMuted)
                    }
                    .padding(.horizontal, 12).padding(.vertical, 8)
                    .background(theme.surface)
                    .clipShape(RoundedRectangle(cornerRadius: 12))
                    .overlay(RoundedRectangle(cornerRadius: 12)
                        .stroke(theme.accent.opacity(0.4), lineWidth: 1))
                    .position(x: centerX, y: centerY)
                }
                .animation(.spring(response: 0.4, dampingFraction: 0.7),
                           value: active)
                .onReceive(timer) { _ in active = (active + 1) % roles.count }
            }
        }
    }
}

// MARK: - Step 4: API-keys nudge

private struct KeysNudgeStep: View {
    @Environment(\.theme) private var theme
    let advance: () -> Void

    var body: some View {
        Group {
            Text("API keys (BYOK)").font(.largeTitle.bold())
            Text("Many collectors call upstream APIs (Shodan, GitHub, Sentinel Hub, …) with your own keys. You paste a key once; it's encrypted per-workspace and injected automatically every time a collector runs.")
                .foregroundStyle(theme.textMuted)

            BYOKFlowDiagram()
                .frame(height: 150)
                .frame(maxWidth: .infinity)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 18))

            Label("Manage anytime under Console → API keys",
                  systemImage: "key.fill")
                .font(.footnote)
                .padding(12)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(theme.surfaceElevated)
                .clipShape(RoundedRectangle(cornerRadius: 10))
            Button("Continue") { advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }
}

/// Animated left-to-right pipeline: a key packet travels through the
/// encrypted vault into a collector and out as live data, so the BYOK
/// path is literally shown moving.
private struct BYOKFlowDiagram: View {
    @Environment(\.theme) private var theme

    private let stages: [(String, String)] = [
        ("Your key",        "key.fill"),
        ("Encrypted vault", "lock.shield.fill"),
        ("Collector",       "antenna.radiowaves.left.and.right"),
        ("Live data",       "globe.asia.australia.fill"),
    ]

    /// Walks the dwell/move schedule for time `t`. Returns the packet's
    /// continuous position (0 … n-1, eased during a move) and `reached`,
    /// the highest stage index the packet has actually *arrived* at this
    /// cycle (advances only when a move completes; resets on loop).
    private func flowState(_ t: Double) -> (pos: Double, reached: Int) {
        let n = stages.count
        let dwell = 0.5, move = 0.7
        let total = Double(n) * dwell + Double(n - 1) * move
        let phase = t.truncatingRemainder(dividingBy: total)
        var acc = 0.0
        for k in 0..<n {
            if phase < acc + dwell { return (Double(k), k) }   // dwell at k
            acc += dwell
            if k < n - 1 {
                if phase < acc + move {
                    let p = (phase - acc) / move
                    let e = p * p * (3 - 2 * p)                 // ease-in-out
                    return (Double(k) + e, k)                   // moving k→k+1
                }
                acc += move
            }
        }
        return (Double(n - 1), n - 1)
    }

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            let st = flowState(t)
            GeometryReader { geo in
                let w = geo.size.width, h = geo.size.height
                let n = stages.count
                let y = h * 0.42
                let xs = (0..<n).map {
                    CGFloat($0) / CGFloat(n - 1) * (w - 64) + 32
                }
                let packetX = xs[0]
                    + (xs[n - 1] - xs[0]) * CGFloat(st.pos / Double(n - 1))
                ZStack {
                    // Travelling key packet — kept first in the ZStack so it
                    // glides *behind* the line, stage circles and labels.
                    Circle().fill(theme.accent)
                        .frame(width: 11, height: 11)
                        .shadow(color: theme.accent.opacity(0.8), radius: 6)
                        .position(x: packetX, y: y)

                    Path { p in
                        p.move(to: CGPoint(x: xs.first!, y: y))
                        p.addLine(to: CGPoint(x: xs.last!, y: y))
                    }
                    .stroke(theme.textMuted.opacity(0.25),
                            style: StrokeStyle(lineWidth: 2, dash: [4, 4]))

                    // Stage circles — centred exactly on the line.
                    ForEach(0..<n, id: \.self) { i in
                        let lit = i <= st.reached
                        Image(systemName: stages[i].1)
                            .font(.system(size: 17, weight: .bold))
                            .foregroundStyle(lit ? Color.black : theme.accent)
                            .frame(width: 44, height: 44)
                            .background(lit ? theme.accent : theme.surface)
                            .clipShape(Circle())
                            .overlay(Circle().stroke(
                                theme.accent.opacity(lit ? 1 : 0.35),
                                lineWidth: 1))
                            .scaleEffect(lit ? 1.12 : 1)
                            .animation(.easeInOut(duration: 0.2), value: lit)
                            .position(x: xs[i], y: y)
                    }

                    // Labels below each circle.
                    ForEach(0..<n, id: \.self) { i in
                        let lit = i <= st.reached
                        Text(stages[i].0).font(.caption2.bold())
                            .foregroundStyle(lit ? theme.text : theme.textMuted)
                            .multilineTextAlignment(.center)
                            .frame(width: 66)
                            .position(x: xs[i], y: y + 38)
                    }
                }
            }
        }
    }
}

// MARK: - Step 5: first collector run

private struct FirstRunStep: View {
    @EnvironmentObject private var settings: AppSettings
    @EnvironmentObject private var registry: LayerRegistry
    @Environment(\.theme) private var theme
    let advance: () -> Void

    @State private var query = ""
    @State private var busy = false

    private let cols = Array(repeating: GridItem(.flexible(), spacing: 10), count: 3)

    /// Every advertised layer, grouped by category, filtered by the search box.
    private var groups: [(category: String, layers: [LayerDef])] {
        let q = query.trimmingCharacters(in: .whitespaces).lowercased()
        return registry.byCategory.compactMap { group in
            let matched = q.isEmpty ? group.layers : group.layers.filter {
                registry.displayName(for: $0).lowercased().contains(q)
                    || $0.id.lowercased().contains(q)
                    || group.category.lowercased().contains(q)
            }
            return matched.isEmpty ? nil : (group.category, matched)
        }
    }

    var body: some View {
        Group {
            Text("First collection").font(.largeTitle.bold())
            Text("Pick any layers you want on the map. We'll activate them so your map and Intel feed aren't empty on first open. Change layers any time under Layers.")
                .foregroundStyle(theme.textMuted)

            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(theme.textMuted)
                TextField("Search layers", text: $query)
                    .compatNoAutocap()
                    .autocorrectionDisabled()
                if !query.isEmpty {
                    Button { query = "" } label: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundStyle(theme.textMuted)
                    }
                }
            }
            .padding(10)
            .background(theme.surfaceElevated)
            .clipShape(RoundedRectangle(cornerRadius: 8))

            ForEach(groups, id: \.category) { group in
                VStack(alignment: .leading, spacing: 8) {
                    Text(group.category.uppercased())
                        .font(.caption2.bold())
                        .foregroundStyle(theme.textMuted)
                    LazyVGrid(columns: cols, spacing: 10) {
                        ForEach(group.layers) { layer in
                            layerChip(layer.id)
                        }
                    }
                }
                .padding(.top, 4)
            }

            Text("\(settings.activeLayerIds.count) layer(s) active")
                .font(.footnote).foregroundStyle(theme.textMuted)

            Button {
                Task { await run() }
            } label: {
                HStack {
                    if busy { ProgressView().tint(.black) }
                    Text(busy ? "Activating…" : "Activate & continue")
                }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(theme.warning)
            .disabled(busy)

            Button("Skip for now") { advance() }
                .font(.footnote).frame(maxWidth: .infinity)
        }
        .task {
            if registry.layers.isEmpty {
                await registry.reload(baseURL: settings.backendBaseURL)
            }
        }
    }

    @ViewBuilder
    private func layerChip(_ id: String) -> some View {
        let on = settings.activeLayerIds.contains(id)
        let tint = registry.color(for: id)
        Button {
            settings.toggleLayer(id)
        } label: {
            VStack(spacing: 6) {
                Image(systemName: registry.symbol(for: id))
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundStyle(on ? Color.black : tint)
                    .frame(width: 40, height: 40)
                    .background(on ? tint : theme.surfaceElevated)
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                Text(LayerRegistry.displayName(forId: id))
                    .font(.system(size: 10))
                    .foregroundStyle(on ? theme.text : theme.textMuted)
                    .lineLimit(1)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(on ? tint : Color.clear, lineWidth: 1.5)
            )
        }
        .buttonStyle(.plain)
    }

    private func run() async {
        busy = true
        defer { busy = false }
        try? await Task.sleep(for: .milliseconds(300))
        advance()
    }
}

// MARK: - Step 6: finish

private struct FinishStep: View {
    @Environment(\.theme) private var theme
    let done: () -> Void

    /// Toggled once in `.onAppear` to fire the one-shot celebratory wiggle.
    @State private var wiggle = false

    var body: some View {
        VStack(spacing: 16) {
            Spacer(minLength: 0)

            Image(systemName: "checkmark.seal.fill")
                .font(.system(size: 64))
                .foregroundStyle(theme.success)
                .phaseAnimator([0.0, -10.0, 8.0, -4.0, 0.0],
                               trigger: wiggle) { view, angle in
                    view.rotationEffect(.degrees(angle))
                } animation: { _ in .easeInOut(duration: 0.375) }
                .onAppear { wiggle = true }
            Text("You're set").font(.largeTitle.bold())
            Text("New data lands on the Map and in the Intel feed as collectors run. Set up alert rules under Console → Alerts to get notified on matches.")
                .foregroundStyle(theme.textMuted)
                .multilineTextAlignment(.center)

            Spacer(minLength: 24)

            Button { done() } label: {
                Text("Enter JapanOSINT")
                    .font(.title2.bold())
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
            }
            .buttonStyle(.borderedProminent)
            .tint(theme.warning)
            .controlSize(.large)

            Spacer(minLength: 0)
        }
        .frame(maxWidth: .infinity)
    }
}
