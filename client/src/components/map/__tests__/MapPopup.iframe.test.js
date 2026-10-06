// Regression (finding 6): the camera popup's <iframe src> accepted a
// javascript: URL — the mlit_river branch returned the scraped URL unchecked
// and the host allowlist ignored the protocol.
import { describe, it, expect } from 'vitest';
import { iframeableCamUrl } from '../MapPopup.jsx';

describe('iframeableCamUrl', () => {
  it('refuses non-http(s) URLs on the mlit_river branch', () => {
    expect(iframeableCamUrl('javascript:alert(document.cookie)', 'mlit_river')).toBeNull();
    expect(iframeableCamUrl('data:text/html,<script>alert(1)</script>', 'mlit_river')).toBeNull();
    expect(iframeableCamUrl(' javascript:alert(1)', 'mlit_river')).toBeNull();
  });

  it('refuses a non-http(s) URL even when its "host" is on the allowlist', () => {
    expect(iframeableCamUrl('javascript://www.river.go.jp/%0aalert(1)', 'other')).toBeNull();
    expect(iframeableCamUrl('ftp://www.windy.com/cam', 'other')).toBeNull();
  });

  it('still embeds http(s) URLs it should', () => {
    expect(iframeableCamUrl('https://www.river.go.jp/kawabou/cam?id=1', 'mlit_river')).toBe('https://www.river.go.jp/kawabou/cam?id=1');
    expect(iframeableCamUrl('http://example.org/live', 'mlit_river')).toBe('http://example.org/live');
    expect(iframeableCamUrl('https://www.windy.com/webcams/1', 'other')).toBe('https://www.windy.com/webcams/1');
    expect(iframeableCamUrl('https://evil.example/cam', 'other')).toBeNull();
  });
});
