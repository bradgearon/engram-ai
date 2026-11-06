import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import RuntimeClient from "./client.js";
class Socket {
  static OPEN = 1;
  constructor() {
    this.readyState = 1;
    this.sent = [];
    Socket.last = this;
  }
  send(body) {
    this.sent.push(JSON.parse(body));
  }
  close() {}
  receive(event) {
    this.onmessage({ data: JSON.stringify(event) });
  }
}
describe("runtime message boundary", () => {
  beforeEach(() => {
    vi.stubGlobal("location", { protocol: "http:", host: "localhost:8787" });
    vi.stubGlobal("WebSocket", Socket);
    vi.useFakeTimers();
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });
  it("waits for the matching acknowledgment while forwarding unrelated events", async () => {
    const events = [];
    const client = new RuntimeClient(
      "/runtime",
      (e) => events.push(e),
      () => {},
    );
    client.connect();
    const reply = client.call("chat.send", "chat.message", {
        chat_id: "one",
        text: "hello",
      }),
      id = Socket.last.sent[0].request_id;
    Socket.last.receive({ type: "chat.message", trace_id: "other", data: {} });
    Socket.last.receive({ type: "token", trace_id: id, text: "hello" });
    Socket.last.receive({
      type: "chat.message",
      trace_id: id,
      data: { saved: true },
    });
    expect((await reply).data.saved).toBe(true);
    expect(events).toHaveLength(3);
  });
  it("surfaces rejection and never retries a mutation on disconnect", async () => {
    const client = new RuntimeClient(
      "/runtime",
      () => {},
      () => {},
    );
    client.connect();
    const reply = client.call("chat.send", "chat.message", {});
    const id = Socket.last.sent[0].request_id;
    Socket.last.receive({
      type: "error",
      request_id: id,
      message: "scope denied",
    });
    await expect(reply).rejects.toThrow("scope denied");
    const interrupted = client.call("chat.send", "chat.message", {});
    Socket.last.onclose();
    await expect(interrupted).rejects.toThrow("journal");
    expect(Socket.last.sent).toHaveLength(2);
  });
  it("reports a timed out acknowledgment without claiming that work failed", async () => {
    const client = new RuntimeClient(
      "/runtime",
      () => {},
      () => {},
    );
    client.connect();
    const reply = client.call("chat.send", "chat.message", {});
    const assertion = expect(reply).rejects.toThrow("may have been accepted");
    await vi.advanceTimersByTimeAsync(20001);
    await assertion;
    expect(Socket.last.sent).toHaveLength(1);
  });
});
