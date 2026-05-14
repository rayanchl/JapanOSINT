import SwiftUI
import WebKit

/// Loads a URL inside a `WKWebView`. Used by `CameraFeedView` for YouTube
/// embeds (`.youtube` mode) and iframeable hosts (`.iframe` mode — Windy,
/// river.go.jp, livecam.asia).
///
/// `onLoadFailure` fires on transport errors (`didFailProvisionalNavigation`,
/// `didFail`) and on HTTP responses with status ≥ 400. Used by the scs.com.ua
/// flow to fall back from a channel-live YouTube iframe to a server snapshot
/// when the channel isn't currently broadcasting (YouTube returns the embed
/// page with an offline-state HTTP error envelope, not an in-page 153 — the
/// 153 only fires on the IFrame Player JS API, which we don't bridge into).
struct CameraWebView: UIViewRepresentable {
    let url: URL
    var onLoadFailure: (() -> Void)? = nil

    func makeCoordinator() -> Coordinator {
        Coordinator(onLoadFailure: onLoadFailure)
    }

    func makeUIView(context: Context) -> WKWebView {
        let cfg = WKWebViewConfiguration()
        cfg.allowsInlineMediaPlayback = true
        cfg.mediaTypesRequiringUserActionForPlayback = []
        let view = WKWebView(frame: .zero, configuration: cfg)
        view.scrollView.isScrollEnabled = false
        view.scrollView.bounces = false
        view.isOpaque = false
        view.backgroundColor = .clear
        view.navigationDelegate = context.coordinator
        view.load(URLRequest(url: url))
        return view
    }

    func updateUIView(_ view: WKWebView, context: Context) {
        context.coordinator.onLoadFailure = onLoadFailure
        if view.url != url {
            view.load(URLRequest(url: url))
        }
    }

    final class Coordinator: NSObject, WKNavigationDelegate {
        var onLoadFailure: (() -> Void)?

        init(onLoadFailure: (() -> Void)?) {
            self.onLoadFailure = onLoadFailure
        }

        func webView(_ webView: WKWebView,
                     didFail navigation: WKNavigation!,
                     withError error: Error) {
            onLoadFailure?()
        }

        func webView(_ webView: WKWebView,
                     didFailProvisionalNavigation navigation: WKNavigation!,
                     withError error: Error) {
            onLoadFailure?()
        }

        func webView(_ webView: WKWebView,
                     decidePolicyFor navigationResponse: WKNavigationResponse,
                     decisionHandler: @escaping (WKNavigationResponsePolicy) -> Void) {
            if let resp = navigationResponse.response as? HTTPURLResponse,
               resp.statusCode >= 400 {
                onLoadFailure?()
                decisionHandler(.cancel)
                return
            }
            decisionHandler(.allow)
        }
    }
}
