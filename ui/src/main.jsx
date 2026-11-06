import "@fontsource/dm-sans/latin-400.css";
import "@fontsource/dm-sans/latin-500.css";
import "@fontsource/dm-sans/latin-600.css";
import "@fontsource/libre-caslon-display/latin-400.css";
import { createRoot } from "react-dom/client";
import Workspace from "./state/Workspace.jsx";
import App from "./App.jsx";
import ErrorBoundary from "./components/ErrorBoundary.jsx";
import "./styles.css";
createRoot(document.getElementById("root")).render(
  <ErrorBoundary>
    <Workspace>
      <App />
    </Workspace>
  </ErrorBoundary>,
);
