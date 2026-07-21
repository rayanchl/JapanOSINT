import SwiftUI
import WebKit
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

/// Loads a URL inside a `WKWebView`. Used by `CameraFeedView` for YouTube
/// embeds (`.youtube` mode) and iframeable hosts (`.iframe` mode — Windy,
/// river.go.jp, livecam.asia).
///
/// `WKWebView` is available on both iOS and macOS, so the same struct backs
/// both platforms — only the representable protocol (`UIViewRepresentable` vs
/// `NSViewRepresentable`) and a couple of view properties (the iOS-only
/// `scrollView`, transparency) differ.
///
/// `onLoadFailure` fires on transport errors (`didFailProvisionalNavigation`,
/// `didFail`) and on HTTP responses with status ≥ 400. Used by the scs.com.ua
/// flow to fall back from a channel-live YouTube iframe to a server snapshot
/// when the channel isn't currently broadcasting (YouTube returns the embed
/// page with an offline-state HTTP error envelope, not an in-page 153 — the
/// 153 only fires on the IFrame Player JS API, which we don't bridge into).
struct CameraWebView {
    let url: URL
    var onLoadFailure: (() -> Void)? = nil

    func makeCoordinator() -> Coordinator {
        Coordinator(onLoadFailure: onLoadFailure)
    }

    fileprivate func makeWebView(coordinator: Coordinator) -> WKWebView {
        let cfg = WKWebViewConfiguration()
        #if os(iOS)
        // iOS defaults to fullscreen playback; macOS already plays inline.
        cfg.allowsInlineMediaPlayback = true
        #endif
        cfg.mediaTypesRequiringUserActionForPlayback = []
        let view = WKWebView(frame: .zero, configuration: cfg)
        #if canImport(UIKit)
        view.scrollView.isScrollEnabled = false
        view.scrollView.bounces = false
        view.isOpaque = false
        view.backgroundColor = .clear
        #elseif canImport(AppKit)
        // macOS WKWebView has no UIScrollView; transparency is controlled by
        // the private `drawsBackground` flag (the documented public API for it
        // ships only on iOS).
        view.setValue(false, forKey: "drawsBackground")
        #endif
        view.navigationDelegate = coordinator
        view.load(URLRequest(url: url))
        return view
    }

    fileprivate func refresh(_ view: WKWebView, coordinator: Coordinator) {
        coordinator.onLoadFailure = onLoadFailure
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

#if canImport(UIKit)
extension CameraWebView: UIViewRepresentable {
    func makeUIView(context: Context) -> WKWebView {
        makeWebView(coordinator: context.coordinator)
    }

    func updateUIView(_ view: WKWebView, context: Context) {
        refresh(view, coordinator: context.coordinator)
    }
}
#elseif canImport(AppKit)
extension CameraWebView: NSViewRepresentable {
    func makeNSView(context: Context) -> WKWebView {
        makeWebView(coordinator: context.coordinator)
    }

    func updateNSView(_ view: WKWebView, context: Context) {
        refresh(view, coordinator: context.coordinator)
    }
}
#endif
