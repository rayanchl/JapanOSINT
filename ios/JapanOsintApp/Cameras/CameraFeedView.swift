import SwiftUI

/// Shared camera feed renderer used by both the map's `CameraPopup` and the
/// `CameraDiscoveryView` list cards. Resolves the appropriate render mode via
/// `CameraFeedResolver` and dispatches to AsyncImage (direct image / snapshot
/// endpoint / proxy), `CameraWebView` (YouTube / iframeable hosts), or
/// `CameraVideoPlayer` (HLS m3u8). Auto-refreshes image-based feeds at the
/// user-chosen cadence.
struct CameraFeedView: View {
    enum Style { case full, compact }

    let directSnapshotURLString: String?
    let pageURLString: String?
    var youtubeID: String? = nil
    var hlsURLString: String? = nil
    var discoveryChannel: String? = nil
    var cameraUID: String? = nil
    /// Original aggregator page URL (e.g. `webcam.scs.com.ua/...`) for cases
    /// where `pageURLString` was upgraded to a YouTube channel-live embed.
    /// Used as the snapshot target when the YouTube iframe fails to render
    /// (channel currently offline / embedding blocked / 4xx).
    var originalPageURLString: String? = nil
    var style: Style = .full
    var showsHeader: Bool = true
    /// Override per-call. When nil (default), uses `settings.cameraRefreshSeconds`
    /// so the user-chosen cadence in the Settings tab applies everywhere.
    var refreshSecondsOverride: TimeInterval? = nil

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var refreshTick: UUID = UUID()
    @State private var lastRefresh: Date = Date()
    @State private var pulse: Bool = false
    /// Set when `CameraWebView` reports a load failure on a YouTube channel-live
    /// iframe; flips this view into the snapshot-of-`original_page_url` path.
    @State private var iframeDidFail: Bool = false

    private var refreshSeconds: TimeInterval {
        refreshSecondsOverride ?? TimeInterval(settings.cameraRefreshSeconds)
    }

    private var mode: FeedMode {
        let resolved = CameraFeedResolver.resolve(
            directHint: directSnapshotURLString,
            pageHint: pageURLString,
            youtubeID: youtubeID,
            hlsHint: hlsURLString,
            discoveryChannel: discoveryChannel,
            cameraUID: cameraUID
        )
        // YouTube-first / snapshot-fallback for channels whose YouTube iframe
        // can fail to render (e.g. scs.com.ua channel-live URLs when the
        // channel isn't broadcasting). Switches to the original aggregator
        // page snapshot once `CameraWebView` reports a load failure.
        if iframeDidFail,
           case .iframe = resolved,
           let original = originalPageURLString, !original.isEmpty {
            return .snapshotEndpoint(pageURL: original)
        }
        return resolved
    }

    /// Image-based modes get the LIVE header + auto-refresh cadence. Iframe /
    /// YouTube / HLS embeds have their own players, so the LIVE chrome would
    /// just be redundant there.
    private var isImageBased: Bool {
        switch mode {
        case .directImage, .snapshotEndpoint, .proxiedImage: return true
        case .youtube, .iframe, .hls, .linkOnly:             return false
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if showsHeader && isImageBased {
                header
            }
            feedView
        }
        .task(id: refreshTaskID) {
            // Only image modes need to be re-fetched; webviews/players manage
            // their own lifecycle.
            guard isImageBased else { return }
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(refreshSeconds))
                if Task.isCancelled { break }
                lastRefresh = Date()
                refreshTick = UUID()
            }
        }
    }

    /// Re-keys the refresh task whenever the resolved mode changes from
    /// non-image to image (or vice-versa) so the loop starts/stops correctly
    /// when callers swap inputs without remounting the view.
    private var refreshTaskID: String {
        isImageBased ? "img" : "embed"
    }

    // MARK: - Header

    private var header: some View {
        HStack(spacing: 6) {
            Circle().fill(theme.success).frame(width: 6, height: 6)
            Text("LIVE")
                .font(.caption2.weight(.bold))
                .foregroundStyle(theme.success)
            Text("· refreshes every \(Int(refreshSeconds))s")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            Spacer()
            Button {
                lastRefresh = Date()
                refreshTick = UUID()
            } label: {
                Image(systemName: "arrow.clockwise")
                    .font(.caption)
            }
            .accessibilityLabel("Refresh feed")
        }
    }

    // MARK: - Feed dispatch

    @ViewBuilder
    private var feedView: some View {
        switch mode {
        case .youtube(let id):
            embed(URL(string: "https://www.youtube.com/embed/\(id)?autoplay=1&mute=1&playsinline=1")!)
        case .iframe(let url):
            embed(url)
        case .hls(let url):
            videoPlayer(url)
        case .directImage(let url):
            asyncImage(at: cacheBust(url))
        case .snapshotEndpoint(let pageURL):
            if let url = snapshotEndpointURL(pageURL) {
                asyncImage(at: url)
            } else {
                placeholder(showing: statusStack(icon: "video.fill", text: "No feed"))
            }
        case .proxiedImage(let uid):
            if let url = proxyEndpointURL(uid) {
                asyncImage(at: url)
            } else {
                placeholder(showing: statusStack(icon: "video.fill", text: "No feed"))
            }
        case .linkOnly:
            placeholder(showing: statusStack(icon: "video.fill", text: "No feed"))
        }
    }

    private func asyncImage(at url: URL) -> some View {
        AsyncImage(url: url, transaction: Transaction(animation: .easeInOut(duration: 0.2))) { phase in
            switch phase {
            case .empty:
                placeholder(showing: ProgressView())
            case .success(let img):
                img.resizable()
                    .aspectRatio(contentMode: style == .full ? .fit : .fill)
                    .frame(maxWidth: .infinity, maxHeight: style == .full ? .infinity : 180)
                    .clipped()
                    .clipShape(RoundedRectangle(cornerRadius: cornerRadius, style: .continuous))
                    .overlay(alignment: .topTrailing) {
                        if !showsHeader {
                            liveDot.padding(8)
                        }
                    }
            case .failure:
                placeholder(showing: statusStack(icon: "video.slash", text: "Couldn't load"))
            @unknown default: EmptyView()
            }
        }
        .id(refreshTick)
    }

    private func embed(_ url: URL) -> some View {
        // Only attach a fallback handler when we actually have somewhere to
        // fall back to. Keeps the failure state from latching for hosts where
        // a snapshot route doesn't exist (Windy, river.go.jp, …).
        let canFallback = (originalPageURLString?.isEmpty == false)
        return CameraWebView(
            url: url,
            onLoadFailure: canFallback ? { iframeDidFail = true } : nil
        )
        .aspectRatio(16.0 / 9.0, contentMode: .fit)
        .frame(maxWidth: .infinity)
        .clipShape(RoundedRectangle(cornerRadius: cornerRadius, style: .continuous))
    }

    private func videoPlayer(_ url: URL) -> some View {
        CameraVideoPlayer(url: url)
            .aspectRatio(16.0 / 9.0, contentMode: .fit)
            .frame(maxWidth: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: cornerRadius, style: .continuous))
    }

    private func statusStack(icon: String, text: String) -> some View {
        VStack(spacing: 4) {
            Image(systemName: icon)
                .font(.title3)
                .foregroundStyle(theme.textMuted)
            Text(text)
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
        }
    }

    private var liveDot: some View {
        Circle()
            .fill(theme.success)
            .frame(width: 8, height: 8)
            .overlay(Circle().stroke(.white.opacity(0.85), lineWidth: 1))
            .opacity(pulse ? 0.4 : 1.0)
            .animation(.easeInOut(duration: 1).repeatForever(autoreverses: true), value: pulse)
            .onAppear { pulse = true }
            .accessibilityLabel("Live")
    }

    private func placeholder<Content: View>(showing content: Content) -> some View {
        Rectangle()
            .fill(theme.surfaceElevated)
            .frame(maxWidth: .infinity, minHeight: minHeight)
            .overlay(content)
            .clipShape(RoundedRectangle(cornerRadius: cornerRadius, style: .continuous))
    }

    // MARK: - URL builders

    private var bustQuery: String {
        "_t=\(Int(lastRefresh.timeIntervalSince1970))"
    }

    private func cacheBust(_ url: URL) -> URL {
        let raw = url.absoluteString
        let sep = raw.contains("?") ? "&" : "?"
        return URL(string: raw + sep + bustQuery) ?? url
    }

    private func snapshotEndpointURL(_ pageURL: String) -> URL? {
        guard !pageURL.isEmpty,
              let encoded = pageURL.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) else {
            return nil
        }
        let base = settings.backendBaseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        return URL(string: "\(base)/api/data/cameras/snapshot?url=\(encoded)&\(bustQuery)")
    }

    private func proxyEndpointURL(_ uid: String) -> URL? {
        guard !uid.isEmpty,
              let encoded = uid.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) else {
            return nil
        }
        let base = settings.backendBaseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        return URL(string: "\(base)/api/data/cameras/proxy?camera_uid=\(encoded)&\(bustQuery)")
    }

    // MARK: - Style helpers

    private var cornerRadius: CGFloat {
        switch style {
        case .full:    return 12
        case .compact: return 8
        }
    }

    private var minHeight: CGFloat {
        switch style {
        case .full:    return 200
        case .compact: return 140
        }
    }
}
