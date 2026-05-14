import Foundation

/// How a camera should be rendered in `CameraFeedView`. Mirrors the branching
/// logic used by the web client (`client/src/components/map/MapPopup.jsx`).
enum FeedMode: Equatable {
    /// AVPlayer-backed live stream (.m3u8). Detected by URL extension or `hls_url`.
    case hls(URL)
    /// YouTube embed inside a WKWebView (autoplay muted).
    case youtube(videoID: String)
    /// Iframeable host (river.go.jp, www.windy.com, livecam.asia) inside a WKWebView.
    case iframe(URL)
    /// AsyncImage with cache-bust. Used for thumbnails and direct snapshot URLs.
    case directImage(URL)
    /// AsyncImage routed through the backend's `/api/data/cameras/snapshot` endpoint
    /// (Puppeteer page screenshot). Only for channels in `snapshotChannels`.
    case snapshotEndpoint(pageURL: String)
    /// AsyncImage routed through the backend's `/api/data/cameras/proxy?camera_uid=…`
    /// — used for Shodan/manual IP cams (plain HTTP, ATS-blocked direct).
    case proxiedImage(cameraUID: String)
    /// No usable feed; CameraPopup shows the "Open camera page" link separately.
    case linkOnly
}

struct CameraFeedResolver {
    /// Hosts whose pages permit X-Frame-Options embedding. Mirrors the web client's
    /// `IFRAMEABLE_HOSTS` set in `MapPopup.jsx`.
    static let iframeableHosts: Set<String> = ["river.go.jp", "www.windy.com", "livecam.asia"]

    /// Discovery channels whose source pages are embed-blocked but where
    /// Puppeteer can capture a usable screenshot. Mirrors `SNAPSHOT_CHANNELS`.
    /// `scs_com_ua` is here as the *fallback* path for the YouTube channel-live
    /// flow: when the underlying channel isn't currently broadcasting, the
    /// channel-live iframe fails to render anything useful, and `CameraFeedView`
    /// switches to a snapshot of `original_page_url` instead.
    static let snapshotChannels: Set<String> = [
        "skylinewebcams", "earthcam", "webcamtaxi", "geocam",
        "worldcams", "webcamera24", "camstreamer",
        "scs_com_ua",
    ]

    /// Discovery channels routed through the backend image proxy because
    /// the upstream URL is plain HTTP to an arbitrary host (ATS would block).
    static let proxiedChannels: Set<String> = ["shodan_api", "manual_ip_seed", "insecam_scrape"]

    /// Direct image URL pattern. Catches conventional image extensions plus
    /// the common surveillance camera idioms (`mjpg`, `snapshot`, `image.cgi`,
    /// `/camera/`). Mirrors the web client's `isImageUrl()` regex.
    private static let imagePattern = #"\.(jpe?g|png|gif|bmp|webp)(\?.*)?$|mjpg|snapshot|image\.cgi|/camera/"#

    private static let imageRegex: NSRegularExpression = {
        // Pattern is a compile-time constant — unwrapping is safe.
        try! NSRegularExpression(pattern: imagePattern, options: [.caseInsensitive])
    }()

    /// YouTube ID extraction. Matches youtube.com/watch?v=, youtu.be/, /embed/, /shorts/.
    /// Negative lookahead for `live_stream` so we don't false-match the literal
    /// string "live_stream" in `embed/live_stream?channel=…` URLs as an 11-char
    /// video ID — those are channel-live embeds, handled by `youtubeChannelRegex`.
    private static let youtubeRegex: NSRegularExpression = {
        try! NSRegularExpression(
            pattern: #"(?:youtube\.com/(?:watch\?v=|embed/(?!live_stream\b)|shorts/)|youtu\.be/)([A-Za-z0-9_-]{11})"#,
            options: [.caseInsensitive])
    }()

    /// YouTube channel-live extraction. Matches `embed/live_stream?channel=UC…`
    /// (24-char canonical channel ID). Used by aggregators like scs.com.ua that
    /// embed a channel rather than a static video ID — YouTube resolves the live
    /// broadcast at iframe-load time.
    private static let youtubeChannelRegex: NSRegularExpression = {
        try! NSRegularExpression(
            pattern: #"youtube\.com/embed/live_stream\?[^\s'"]*?channel=(UC[A-Za-z0-9_-]{22})"#,
            options: [.caseInsensitive])
    }()

    /// Resolve the best `FeedMode` for a camera record.
    ///
    /// - Parameters:
    ///   - directHint: A pre-resolved direct image URL (from `thumbnail_url` /
    ///     `snapshot_url` / `image` / etc. — caller does the property lookup).
    ///   - pageHint: The camera's `url` field (page or embed URL).
    ///   - youtubeID: Pre-extracted YouTube video ID, if the collector tagged one.
    ///   - hlsHint: A `.m3u8` URL if the collector emitted one explicitly.
    ///   - discoveryChannel: First entry from `discovery_channels`. Used only as
    ///     a tie-breaker (gates `.snapshotEndpoint` and `.proxiedImage`).
    ///   - cameraUID: Required for `.proxiedImage`.
    static func resolve(directHint: String?,
                        pageHint: String?,
                        youtubeID: String? = nil,
                        hlsHint: String? = nil,
                        discoveryChannel: String? = nil,
                        cameraUID: String? = nil) -> FeedMode {

        // 1. HLS — cheapest and most specific check first.
        if let raw = hlsHint, let url = URL(string: raw) { return .hls(url) }
        if let raw = pageHint, isHLS(raw), let url = URL(string: raw) { return .hls(url) }

        // 2a. YouTube channel-live embed (e.g. scs.com.ua). Must run before
        //     the video-ID check: `embed/live_stream` would otherwise be
        //     captured as a fake 11-char ID by some upstream consumers.
        //     Routed as `.iframe` because YouTube serves channel-live URLs in
        //     iframes natively — no separate FeedMode needed.
        if let raw = pageHint,
           let cid = extractYouTubeChannel(raw),
           let url = URL(string: "https://www.youtube.com/embed/live_stream?channel=\(cid)&autoplay=1&mute=1&playsinline=1") {
            return .iframe(url)
        }

        // 2b. YouTube — explicit ID, or extract from pageHint.
        if let id = youtubeID, !id.isEmpty, id != "live_stream" {
            return .youtube(videoID: id)
        }
        if let raw = pageHint, let id = extractYouTubeID(raw) {
            return .youtube(videoID: id)
        }

        // 3. Iframeable hosts (Windy, river.go.jp, livecam.asia) or mlit_river channel.
        if let raw = pageHint,
           let url = URL(string: raw),
           let host = url.host?.lowercased(),
           iframeableHosts.contains(host) || iframeableHosts.contains(stripWWW(host)) {
            return .iframe(url)
        }
        if discoveryChannel == "mlit_river",
           let raw = pageHint,
           let url = URL(string: raw) {
            return .iframe(url)
        }

        let chanLower = discoveryChannel?.lowercased() ?? ""

        // 4. Direct image hint (already resolved from thumbnail_url/snapshot_url/...).
        //    Skip when the channel is routed through the backend image proxy — its
        //    "direct" URLs (insecam camera-IPs, …) are blocked by ATS, so the proxy
        //    is the working path even though we have a directHint.
        if let raw = directHint, !raw.isEmpty, let url = URL(string: raw),
           !proxiedChannels.contains(chanLower) {
            return .directImage(url)
        }

        // 5. URL itself looks like a direct image (mjpg, snapshot, .jpg, /camera/, …).
        if let raw = pageHint, looksLikeImage(raw), let url = URL(string: raw) {
            return .directImage(url)
        }

        // 6. Channel routes through the backend image proxy (Shodan, manual IP seeds, insecam).
        if proxiedChannels.contains(chanLower),
           let uid = cameraUID, !uid.isEmpty {
            return .proxiedImage(cameraUID: uid)
        }

        // 7. Channel uses the headless-browser page-screenshot endpoint.
        if snapshotChannels.contains(chanLower),
           let raw = pageHint, !raw.isEmpty {
            return .snapshotEndpoint(pageURL: raw)
        }

        // 8. Nothing usable — CameraPopup will show the "Open camera page" link.
        return .linkOnly
    }

    // MARK: - Helpers

    static func extractYouTubeID(_ url: String) -> String? {
        let range = NSRange(url.startIndex..<url.endIndex, in: url)
        guard let m = youtubeRegex.firstMatch(in: url, options: [], range: range),
              m.numberOfRanges >= 2,
              let r = Range(m.range(at: 1), in: url) else { return nil }
        return String(url[r])
    }

    static func extractYouTubeChannel(_ url: String) -> String? {
        let range = NSRange(url.startIndex..<url.endIndex, in: url)
        guard let m = youtubeChannelRegex.firstMatch(in: url, options: [], range: range),
              m.numberOfRanges >= 2,
              let r = Range(m.range(at: 1), in: url) else { return nil }
        return String(url[r])
    }

    static func looksLikeImage(_ url: String) -> Bool {
        let range = NSRange(url.startIndex..<url.endIndex, in: url)
        return imageRegex.firstMatch(in: url, options: [], range: range) != nil
    }

    static func isHLS(_ url: String) -> Bool {
        let lower = url.lowercased()
        // Strip query before the extension check.
        let path = lower.split(separator: "?").first.map(String.init) ?? lower
        return path.hasSuffix(".m3u8")
    }

    private static func stripWWW(_ host: String) -> String {
        host.hasPrefix("www.") ? String(host.dropFirst(4)) : host
    }
}
