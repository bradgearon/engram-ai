// Local opt-in telemetry. No polling, remote analytics, or dependency.
let socket, options, started = false, dropped = 0;
let buffered = [];
const pageSession = crypto.randomUUID();
const base = () => ({ page_session: pageSession, url: location.pathname, time_ms: Date.now() });
function safe(value) {
  const seen = new WeakSet();
  return JSON.stringify(value, (key, item) => {
    if (item instanceof Error) return { name: item.name, message: item.message, stack: item.stack };
    if (item && typeof item === "object") {
      if (seen.has(item)) return "[circular]";
      seen.add(item);
    }
    return item;
  });
}
function flush() {
  if (socket?.readyState !== WebSocket.OPEN || !options) return;
  if (!options.enabled) { buffered = []; dropped = 0; return; }
  if (dropped && socket.bufferedAmount < 65536) {
    socket.send(safe({ ...base(), kind: "browser.trace_dropped", payload: { count: dropped }, error: true })); dropped = 0;
  }
  while (buffered.length && socket.bufferedAmount < 65536) {
    const row = buffered.shift();
    if (options.level === "errors" && !row.error) continue;
    if (!options.include_content) row.payload = { content_omitted: true };
    const data = safe(row);
    socket.send(data.length > 12000 ? safe({ ...base(), kind: "browser.trace_oversize", error: true,
      payload: { original_kind: row.kind, characters: data.length, omitted: true } }) : data);
  }
  if (buffered.length) { dropped += buffered.length; buffered = []; }
}
export default function trace(kind, payload = {}, error = false) {
  try {
    if (options && !options.enabled) return;
    const row = { ...base(), kind, payload, error };
    // Bound telemetry even while the diagnostics connection is unavailable.
    if (safe(row).length > 12000) row.payload = { oversized: true, original_kind: kind, message: "Details exceeded browser trace record limit; consult native trace." };
    if (buffered.length >= 200) { buffered.shift(); dropped++; }
    buffered.push(row); flush();
  } catch { dropped++; }
}
function connect() {
  if (socket && socket.readyState < WebSocket.CLOSING) return;
  options = undefined;
  socket = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/diagnostics`);
  socket.onmessage = ({ data }) => {
    try {
      const hello = JSON.parse(data);
      if (hello.type === "diagnostics.config") {
        options = hello.settings;
        trace("browser.started", { user_agent: navigator.userAgent, version: "0.7.3" }); flush();
      }
    } catch { /* Never recursively log diagnostics transport errors. */ }
  };
}
export function startDiagnostics() {
  if (started) return;
  started = true; connect();
  window.addEventListener("online", connect);
  window.addEventListener("focus", connect);
  window.addEventListener("error", (e) => trace("browser.error", { message: e.message, file: e.filename, line: e.lineno, column: e.colno, error: e.error }, true));
  window.addEventListener("unhandledrejection", (e) => trace("browser.unhandled_rejection", { reason: e.reason }, true));
  for (const method of ["warn", "error"]) {
    const original = console[method].bind(console);
    console[method] = (...args) => { trace(`browser.console.${method}`, { args }, true); original(...args); };
  }
  window.addEventListener("pagehide", () => { trace("browser.pagehide"); socket?.close(); });
  window.addEventListener("pageshow", connect);
}
