import trace from "./diagnostics.js";
export default class RuntimeClient {
  constructor(path, onEvent, onStatus) {
    this.path = path;
    this.onEvent = onEvent;
    this.onStatus = onStatus;
    this.pending = new Map();
  }
  connect() {
    this.close();
    trace("websocket.connecting", { path: this.path });
    this.onStatus("connecting");
    this.socket = new WebSocket(
      `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}${this.path}`,
    );
    this.socket.onopen = () => { trace("websocket.connected", { path: this.path }); this.onStatus("connected"); };
    this.socket.onmessage = ({ data }) => {
      let event;
      try {
        event = JSON.parse(data);
        if (!event || typeof event.type !== "string")
          throw Error("Malformed runtime event");
      } catch (error) {
        trace("websocket.malformed", { path: this.path, data, error }, true);
        this.onEvent({ type: "error", message: error.message });
        return;
      }
      trace("websocket.event", { path: this.path, type: event.type, request_id: event.request_id || event.trace_id, instance_id: event.instance_id, seq: event.seq, ...(event.type === "error" ? { message: event.message } : {}) }, event.type === "error");
      const id = event.trace_id || event.request_id,
        request = this.pending.get(id);
      if (
        request &&
        (event.type === request.expected || event.type === "error")
      ) {
        clearTimeout(request.timer);
        this.pending.delete(id);
        if (event.type === "error")
          request.reject(Error(event.message || "Request rejected"));
        else request.resolve(event);
      }
      this.onEvent(event);
    };
    this.socket.onclose = (event) => {
      trace("websocket.closed", { path: this.path, code: event.code, reason: event.reason }, true);
      this.onStatus("disconnected");
      this.rejectPending(
        "Connection interrupted. Accepted work stays in the journal; reconnect to recover it.",
      );
    };
    this.socket.onerror = () => this.onStatus("disconnected");
  }
  rejectPending(message) {
    for (const r of this.pending.values()) {
      clearTimeout(r.timer);
      r.reject(Error(message));
    }
    this.pending.clear();
  }
  call(type, expected, values = {}) {
    if (this.socket?.readyState !== WebSocket.OPEN)
      return Promise.reject(Error("Runtime is disconnected"));
    const request_id = crypto.randomUUID();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        trace("websocket.timeout", { path: this.path, request_id, type, expected }, true);
        this.pending.delete(request_id);
        reject(
          Error(
            "Request acknowledgement timed out. Refresh before retrying; work may have been accepted.",
          ),
        );
      }, 20000);
      this.pending.set(request_id, { resolve, reject, timer, expected });
      trace("websocket.request", { path: this.path, schema: 1, type, request_id, ...values });
      this.socket.send(
        JSON.stringify({ schema: 1, type, request_id, ...values }),
      );
    });
  }
  send(message) {
    if (this.socket?.readyState !== WebSocket.OPEN)
      throw Error("Workshop is disconnected");
    trace("websocket.send", { path: this.path, message });
    this.socket.send(JSON.stringify(message));
  }
  close() {
    if (this.socket) {
      this.socket.onclose = null;
      this.socket.close();
    }
    this.rejectPending("Connection closed");
  }
}
