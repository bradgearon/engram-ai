import { Component } from "react";

export default class ErrorBoundary extends Component {
  state = { error: null };
  static getDerivedStateFromError(error) {
    return { error };
  }
  render() {
    if (this.state.error) {
      return (
        <main className="page" role="alert">
          <h1>The workspace could not render.</h1>
          <p>Your saved conversations remain on the runtime.</p>
          <pre className="json-view">{this.state.error.message}</pre>
          <button className="primary" onClick={() => location.reload()}>
            Reload workspace
          </button>
        </main>
      );
    }
    return this.props.children;
  }
}
