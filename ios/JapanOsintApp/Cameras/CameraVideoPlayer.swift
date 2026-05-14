import SwiftUI
import AVKit

/// AVPlayer-backed live stream view. Used by `CameraFeedView` for `.hls` mode
/// (m3u8 streams). Plays muted on appear; user can unmute via the inline
/// playback controls.
struct CameraVideoPlayer: UIViewControllerRepresentable {
    let url: URL

    func makeUIViewController(context: Context) -> AVPlayerViewController {
        let vc = AVPlayerViewController()
        vc.showsPlaybackControls = true
        vc.entersFullScreenWhenPlaybackBegins = false
        vc.allowsPictureInPicturePlayback = false
        vc.videoGravity = .resizeAspect
        configurePlayer(on: vc, with: url)
        return vc
    }

    func updateUIViewController(_ vc: AVPlayerViewController, context: Context) {
        let current = (vc.player?.currentItem?.asset as? AVURLAsset)?.url
        if current != url {
            configurePlayer(on: vc, with: url)
        }
    }

    private func configurePlayer(on vc: AVPlayerViewController, with url: URL) {
        let player = AVPlayer(url: url)
        player.isMuted = true
        player.automaticallyWaitsToMinimizeStalling = true
        vc.player = player
        player.play()
    }
}
