export default class RuntimeClient {
  constructor(path, onEvent, onStatus) {
    this.path = path;
    this.onEvent = onEvent;
    this.onStatus = onStatus;
    this.pending = new Map();
  }
  connect() {
    this.close();
    this.onStatus("connecting");
    this.socket = new WebSocket(
      `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}${this.path}`,
    );
    this.socket.onopen = () => this.onStatus("connected");
    this.socket.onmessage = ({ data }) => {
      let event;
      try {
        event = JSON.parse(data);
        if (!event || typeof event.type !== "string")
          throw Error("Malformed runtime event");
      } catch (error) {
        this.onEvent({ type: "error", message: error.message });
        return;
      }
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
    this.socket.onclose = () => {
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
        this.pending.delete(request_id);
        reject(
          Error(
            "Request acknowledgement timed out. Refresh before retrying; work may have been accepted.",
          ),
        );
      }, 20000);
      this.pending.set(request_id, { resolve, reject, timer, expected });
      this.socket.send(
        JSON.stringify({ schema: 1, type, request_id, ...values }),
      );
    });
  }
  send(message) {
    if (this.socket?.readyState !== WebSocket.OPEN)
      throw Error("Workshop is disconnected");
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
