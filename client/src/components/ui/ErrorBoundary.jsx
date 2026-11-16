import React from 'react';

/**
 * Catch a render-time exception in a subtree and show it as a failure, rather
 * than letting React unmount the whole app.
 *
 * Why it exists (2026-11-07): there was no error boundary anywhere in the
 * client. The transit station route answered HTTP 200 with
 * `{"error":"station not found"}`, StationPopup read `data.station.name`, the
 * TypeError propagated out of MapPopup (rendered inside MapPage) and one click
 * on a station blanked the entire application. The popups now treat a body
 * `error` as a failure themselves; this is the backstop for the next shape
 * nobody anticipated.
 *
 *   <ErrorBoundary label="Map popup" resetKey={feature}>…</ErrorBoundary>
 *
 * `resetKey` clears the caught error when it changes (a new popup, a new
 * route), so one bad feature does not poison every later one. `compact`
 * renders a one-line notice suitable for a small container.
 */
export default class ErrorBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = { error: null };
    this.reset = this.reset.bind(this);
  }

  static getDerivedStateFromError(error) {
    return { error };
  }

  componentDidCatch(error, info) {
    // Surface it for whoever has a console open; the UI states it too.
    // eslint-disable-next-line no-console
    console.error(`[ErrorBoundary${this.props.label ? `: ${this.props.label}` : ''}]`, error, info?.componentStack);
  }

  componentDidUpdate(prev) {
    if (this.state.error && prev.resetKey !== this.props.resetKey) this.reset();
  }

  reset() { this.setState({ error: null }); }

  render() {
    const { error } = this.state;
    if (!error) return this.props.children;
    if (typeof this.props.fallback === 'function') return this.props.fallback(error, this.reset);
    const msg = error?.message || String(error);
    const label = this.props.label || 'This view';
    if (this.props.compact) {
      return (
        <div className="text-xs text-neon-red" role="alert">
          {label} failed to render: <span className="font-mono">{msg}</span>
        </div>
      );
    }
    return (
      <div className="m-4 rounded-md border border-neon-red/40 bg-neon-red/10 px-3 py-2 text-xs text-neon-red" role="alert">
        <div className="font-semibold">{label} failed to render</div>
        <div className="font-mono mt-0.5 break-all">{msg}</div>
        <div className="mt-1 text-neon-red/70">The rest of the application is still running.</div>
        <button type="button" className="mt-2 underline" onClick={this.reset}>Try again</button>
      </div>
    );
  }
}
