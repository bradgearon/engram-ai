import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { existsSync } from "node:fs";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
const root = fileURLToPath(new URL("..", import.meta.url));
// Optional local development harness. Never included in a production bundle.
const localRuntime = {
  name: "wds-local-development",
  configureServer(server) {
    if (process.env.VITEST || !existsSync(root + "/configs/ui.local.json"))
      return;
    const child = spawn(
      process.env.PYTHON || "python3",
      ["tools/dev_runtime.py", "--config", "configs/ui.local.json"],
      { cwd: root, stdio: "inherit" },
    );
    child.on("error", (e) => server.config.logger.error(e.message));
    server.httpServer?.once("close", () => child.kill("SIGTERM"));
  },
};
export default defineConfig({
  plugins: [react(), localRuntime],
  server: {
    proxy: {
      "/runtime": { target: "ws://127.0.0.1:8787", ws: true },
      "/workshop": { target: "ws://127.0.0.1:8787", ws: true },
    },
  },
  build: { sourcemap: true },
});
