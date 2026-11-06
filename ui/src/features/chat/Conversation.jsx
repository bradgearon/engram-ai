import { useEffect, useRef, useState } from "react";
import {
  ArrowUp,
  Wind,
  UserRound,
  ChevronRight,
  MoreHorizontal,
  MessageSquare,
  Leaf,
  Clock,
  Bookmark,
  Square,
  Settings2,
  ArrowUpRight,
  Check,
} from "lucide-react";
import { useWorkspace } from "../../state/Workspace.jsx";
import { stamp, shortKey } from "../../lib/format.js";
import Mark from "../../components/Mark.jsx";
import Modal from "../../components/Modal.jsx";

export default function Conversation({ chat, root, onNew, onInspect }) {
  const w = useWorkspace(),
    [scope, setScope] = useState(null),
    [text, setText] = useState(""),
    [busy, setBusy] = useState(false),
    [before, setBefore] = useState(null),
    [settings, setSettings] = useState(false),
    [feedback, setFeedback] = useState(null),
    [source, setSource] = useState("");
  const bottom = useRef(),
    scroll = useRef(),
    follow = useRef(true);
  useEffect(() => {
    if (w.status === "connected")
      w.run(async () => {
        const r = await w.call("heart.inspect", "heart.snapshot", {
          root_id: root,
        });
        setScope(r.data);
      });
  }, [root, w.revision, w.status]);
  useEffect(() => {
    setSource("");
    setText("");
    if (chat) {
      w.run(async () => setBefore(await w.history(chat.id)));
      localStorage.setItem("read-" + chat.id, String(Date.now()));
    }
  }, [chat?.id]);
  const messages = w.messages[chat?.id] || [],
    live = w.streams[chat?.pending_instance] || "";
  useEffect(() => {
    if (follow.current)
      bottom.current?.scrollIntoView({ behavior: "smooth", block: "end" });
    if (chat) localStorage.setItem("read-" + chat.id, String(Date.now()));
  }, [messages.length, live]);
  const souls = scope?.souls || [];
  const selected =
    souls.find((s) => s.soul_id === chat?.soul_id) ||
    souls.find((s) => s.soul_id === root);
  const last = [...messages].reverse().find((m) => m.role === "assistant");
  const profile = selected?.expression || {};
  const wind = w.packages.packages.find(
    (p) => p.key === (chat?.wind_key || w.packages.active.general),
  );
  const update = (values) =>
    w.run(() =>
      w.call("chat.update", "chat.saved", {
        chat_id: chat.id,
        expected_version: chat.version,
        ...values,
      }),
    );
  async function send(e) {
    e.preventDefault();
    if (!text.trim() || busy) return;
    setBusy(true);
    const draft = text;
    const ok = await w.run(() =>
      w.call("chat.send", "chat.message", {
        chat_id: chat.id,
        text: draft,
        ...(source ? { source_root: source } : {}),
      }),
    );
    if (ok) setText("");
    setBusy(false);
  }
  return (
    <div className="conversation-layout">
      <section className="conversation">
        {chat && (
          <div className="focus-bar">
            <div className="segmented">
              <button
                className={chat.mode === "host" ? "active" : ""}
                onClick={() => update({ mode: "host" })}
                disabled={!!chat.pending_signal}
              >
                <UserRound size={14} />
                AI host
              </button>
              <button
                className={chat.mode === "wind" ? "active" : ""}
                onClick={() => update({ mode: "wind" })}
                disabled={!!chat.pending_signal}
              >
                <Wind size={15} />
                Let a Wind speak
              </button>
            </div>
            <label className="inline-select">
              <span>Soul</span>
              <select
                aria-label="Focused soul"
                value={chat.soul_id}
                onChange={(e) => update({ soul_id: e.target.value })}
                disabled={!!chat.pending_signal}
              >
                <option value="">Automatic</option>
                {souls
                  .filter((s) => s.soul_id !== "workspace-user")
                  .map((s) => (
                    <option value={s.soul_id} key={s.soul_id}>
                      {s.name}
                    </option>
                  ))}
              </select>
            </label>
            <label className="inline-select compact-wind">
              <span>Wind</span>
              <select
                aria-label="Focused Wind"
                value={chat.wind_key}
                onChange={(e) => update({ wind_key: e.target.value })}
                disabled={!!chat.pending_signal}
              >
                <option value="">
                  {wind?.manifest?.name ||
                    wind?.manifest?.id ||
                    "Active policy"}{" "}
                  · active
                </option>
                {w.packages.packages
                  .filter((p) => p.manifest?.domain === "general")
                  .map((p) => (
                    <option key={p.key} value={p.key}>
                      {p.manifest.name || p.manifest.id}
                    </option>
                  ))}
              </select>
            </label>
            <button
              className="icon"
              aria-label="Conversation settings"
              onClick={() => setSettings(true)}
            >
              <Settings2 size={17} />
            </button>
          </div>
        )}
        <div
          className="messages"
          ref={scroll}
          onScroll={() => {
            const e = scroll.current;
            follow.current =
              e.scrollHeight - e.scrollTop - e.clientHeight < 100;
          }}
        >
          {!messages.length && (
            <div className="welcome">
              <div className="eyebrow">
                <span className="tiny-rule" />A LITTLE ROOM TO THINK
              </div>
              <h1>
                What shall we
                <br />
                make of this?
              </h1>
              <p>
                Bring a question, a half-formed idea, or something worth
                understanding.
              </p>
              <div className="welcome-note">
                <Leaf size={17} />
                <span>One conversation. Many ways to grow.</span>
              </div>
              {!chat && (
                <button
                  className="primary"
                  onClick={onNew}
                  disabled={w.status !== "connected"}
                >
                  <MessageSquare size={16} />
                  Start a conversation
                </button>
              )}
            </div>
          )}
          {before != null && (
            <button
              className="text-button history-button"
              onClick={() =>
                w.run(async () => setBefore(await w.history(chat.id, before)))
              }
            >
              Load earlier messages
            </button>
          )}
          {messages.map((m) => (
            <article key={m.id} className={`message ${m.role}`}>
              <div className="message-avatar">
                {m.role === "user" ? (
                  <span className="avatar">Y</span>
                ) : (
                  <Mark small />
                )}
              </div>
              <div className="message-body">
                <div className="message-byline">
                  <b>
                    {m.role === "user"
                      ? "You"
                      : m.speaker?.display_name ||
                        m.speaker?.selected_soul ||
                        "AI host"}
                  </b>
                  <time>{stamp(m.time_ms)}</time>
                  {m.kind === "contribution" && (
                    <span className="pill">
                      <Leaf size={11} />
                      Quiet contribution
                    </span>
                  )}
                  {m.kind === "reflection" && (
                    <span className="pill">Saved reflection</span>
                  )}
                </div>
                {m.role === "assistant" && (
                  <div className="attribution">
                    <span>{shortKey(m.wind_key)}</span>
                    {m.origin_project !== root && (
                      <span className="guest">
                        <ArrowUpRight size={12} />
                        From{" "}
                        {w.snapshot.projects.find(
                          (p) => p.id === m.origin_project,
                        )?.name || m.origin_project}
                      </span>
                    )}
                    {m.memory_sources?.some(
                      (s) => s.chat_id && s.chat_id !== chat.id,
                    ) && (
                      <span className="guest">
                        Includes memory from another conversation
                      </span>
                    )}
                    {m.speaker?.idea_key && (
                      <span>Engram-selected speaker</span>
                    )}
                  </div>
                )}
                <div className="message-text">
                  {m.text || "No complete output was produced."}
                </div>
                {["failed", "cancelled", "exhausted"].includes(m.status) && (
                  <div className="message-outcome">
                    {m.status === "exhausted"
                      ? "Evaluator did not pass this response"
                      : m.status}{" "}
                    {m.error}
                  </div>
                )}
                {m.role === "assistant" && (
                  <div className="message-actions">
                    <button onClick={() => setFeedback(m)}>
                      <Bookmark size={13} />
                      Reflect on this
                    </button>
                    <button onClick={onInspect}>
                      <NetworkIcon />
                      Trace understanding
                    </button>
                  </div>
                )}
              </div>
            </article>
          ))}
          {chat?.pending_signal && (
            <article className="message assistant">
              <Mark small />
              <div className="message-body">
                <div className="message-byline">
                  <b>{selected?.name || "AI host"}</b>
                  <span className="busy-dot" />
                  Responding
                </div>
                <div className="message-text">
                  {chat.pending_status === "paused"
                    ? "This response was paused when the runtime restarted."
                    : live || "Considering the current understanding…"}
                  {chat.pending_status === "paused" && (
                    <button
                      className="secondary"
                      onClick={() =>
                        w.run(() =>
                          w.call("resume", "wind.checkpoint", {
                            instance_id: chat.pending_instance,
                          }),
                        )
                      }
                    >
                      Resume response
                    </button>
                  )}
                </div>
              </div>
            </article>
          )}
          <div ref={bottom} />
        </div>
        {chat && (
          <div className="composer-area">
            {source && (
              <div className="guest-source">
                Guest voice from{" "}
                {w.snapshot.projects.find((p) => p.id === source)?.name}. This
                message will enter that project’s memory.
              </div>
            )}
            <form className="composer" onSubmit={send}>
              <textarea
                aria-label="Message"
                placeholder={
                  chat.mode === "host"
                    ? "Talk with your AI host…"
                    : "Give the Wind something to work with…"
                }
                value={text}
                onChange={(e) => setText(e.target.value)}
                onKeyDown={(e) => {
                  if (
                    e.key === "Enter" &&
                    !e.shiftKey &&
                    !e.nativeEvent.isComposing
                  ) {
                    e.preventDefault();
                    if (text.trim() && !chat.pending_signal && !busy)
                      e.currentTarget.form.requestSubmit();
                  }
                }}
                rows={3}
              />
              <div className="composer-bottom">
                <span>
                  <span className="live-dot" />
                  {chat.mode === "host" ? "AI host" : "Wind"}
                  <span className="separator">·</span>
                  {chat.soul_id
                    ? selected?.name
                    : "Soul selected automatically"}
                </span>
                <div>
                  {chat.pending_instance && (
                    <button
                      type="button"
                      className="icon"
                      aria-label="Stop response"
                      onClick={() =>
                        w.run(() =>
                          w.call("cancel", "wind.checkpoint", {
                            instance_id: chat.pending_instance,
                          }),
                        )
                      }
                    >
                      <Square size={15} />
                    </button>
                  )}
                  <button
                    className="send"
                    aria-label="Send message"
                    disabled={
                      !text.trim() ||
                      busy ||
                      !!chat.pending_signal ||
                      w.status !== "connected"
                    }
                  >
                    <ArrowUp size={19} />
                  </button>
                </div>
              </div>
            </form>
            <div className="composer-hint">
              <span>Enter to send · Shift + Enter for a new line</span>
              <span>
                <Leaf size={12} />
                {chat.background
                  ? "Quiet contributions welcome"
                  : "Quiet contributions paused"}
              </span>
            </div>
          </div>
        )}
      </section>
      <aside className="context-rail">
        <div className="rail-title">
          IN THIS CONVERSATION
          <span className="live-dot" />
        </div>
        <div className="identity">
          <span className="soul-symbol">✳</span>
          <p className="eyebrow">FOCUSED SOUL</p>
          <h2>{selected?.name || "AI host"}</h2>
          <p>{selected?.purpose || "Your understanding, taking shape."}</p>
          <div className="tags">
            {(profile.presence || ["warm", "attentive"]).map((t) => (
              <span key={t}>{t}</span>
            ))}
          </div>
          <dl>
            <dt>Form</dt>
            <dd>{profile.form || "Unspecified"}</dd>
            <dt>Gender</dt>
            <dd>{profile.gender_type || "Unspecified"}</dd>
          </dl>
          <button className="text-button" onClick={onInspect}>
            View soul status <ChevronRight size={15} />
          </button>
        </div>
        <div className="rail-block">
          <p className="eyebrow">WIND</p>
          <h3>
            <Wind size={17} />
            {wind?.manifest?.name || wind?.manifest?.id || "Automatic"}
          </h3>
          <p>
            {chat?.mode === "host"
              ? "Host conversation uses the foundation directly."
              : "The selected policy guides response movement."}
          </p>
          {chat && (
            <select
              aria-label="Focused Wind"
              value={chat.wind_key}
              onChange={(e) => update({ wind_key: e.target.value })}
              disabled={!!chat.pending_signal}
            >
              <option value="">Active general Wind</option>
              {w.packages.packages
                .filter((p) => !p.manifest.action_domain)
                .map((p) => (
                  <option key={p.key} value={p.key}>
                    {p.manifest.id} · v{p.manifest.policy_version}
                  </option>
                ))}
            </select>
          )}
          <small>Action Winds are available in learning.</small>
        </div>
        <div className="rail-block quiet">
          <Leaf size={18} />
          <h3>A quiet presence</h3>
          <p>
            Useful follow-ups can arrive here while you’re away. Their speaker
            and source are always shown.
          </p>
          {chat && (
            <button
              className="text-button"
              onClick={() => update({ background: !chat.background })}
              disabled={!!chat.pending_signal}
            >
              {chat.background ? "Pause contributions" : "Allow contributions"}
            </button>
          )}
        </div>
      </aside>
      {settings && (
        <Modal title="Conversation settings" onClose={() => setSettings(false)}>
          <form
            onSubmit={async (e) => {
              e.preventDefault();
              const f = new FormData(e.currentTarget);
              const ok = await update({
                title: f.get("title"),
                ...(f.get("project") !== root
                  ? { project_id: f.get("project") }
                  : {}),
              });
              if (ok) setSettings(false);
            }}
          >
            <label>
              Title
              <input
                name="title"
                defaultValue={chat.title}
                required
                maxLength={120}
              />
            </label>
            <label>
              Project
              <select name="project" defaultValue={root}>
                {w.snapshot.projects.map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.name}
                  </option>
                ))}
              </select>
            </label>
            <p className="muted">
              Moving a chat keeps previous message attribution. New replies use
              the destination project’s memory.
            </p>
            <footer>
              <button className="primary" disabled={!!chat.pending_signal}>
                Save changes
              </button>
            </footer>
          </form>
          <hr />
          <h3>Invite another project’s voice</h3>
          <p className="muted">
            Only the message you send is shared with the invited project.
          </p>
          <select
            aria-label="Guest project"
            value={source}
            onChange={async (e) => {
              const id = e.target.value;
              if (id && !chat.guests.includes(id))
                await update({ guests: [...chat.guests, id] });
              setSource(id);
            }}
          >
            <option value="">Use this project</option>
            {w.snapshot.projects
              .filter((p) => p.id !== root)
              .map((p) => (
                <option value={p.id} key={p.id}>
                  {p.name}
                </option>
              ))}
          </select>
        </Modal>
      )}
      {feedback && (
        <Modal title="Save a reflection" onClose={() => setFeedback(null)}>
          <p className="muted">
            What should be remembered for the next attempt?
          </p>
          <form
            onSubmit={async (e) => {
              e.preventDefault();
              const f = new FormData(e.currentTarget);
              const ok = await w.run(() =>
                w.call("chat.reflect", "chat.message", {
                  chat_id: chat.id,
                  text: f.get("text"),
                  ...(feedback?.instance_id
                    ? { instance_id: feedback.instance_id }
                    : {}),
                }),
              );
              if (ok) setFeedback(null);
            }}
          >
            <label>
              Your feedback
              <textarea name="text" required autoFocus maxLength={2048} />
            </label>
            <footer>
              <button className="primary">
                <Bookmark size={15} />
                Save to memory
              </button>
            </footer>
          </form>
        </Modal>
      )}
    </div>
  );
}
function NetworkIcon() {
  return <span aria-hidden="true">⌘</span>;
}
