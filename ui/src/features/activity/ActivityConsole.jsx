import { useState } from "react";
import {
  Activity,
  Pause,
  Play,
  SlidersHorizontal,
  ArrowUpRight,
  Clock,
  Leaf,
} from "lucide-react";
import { useWorkspace } from "../../state/Workspace.jsx";
import { stamp, label } from "../../lib/format.js";
import Modal from "../../components/Modal.jsx";
import MeshPanel from "./MeshPanel.jsx";
import AssemblyPanel from "./AssemblyPanel.jsx";
import KnowledgePanel from "./KnowledgePanel.jsx";

export default function ActivityConsole({ root, chat }) {
  const w = useWorkspace(),
    [filter, setFilter] = useState("scope"),
    [selected, setSelected] = useState(null),
    [settings, setSettings] = useState(false),
    [review, setReview] = useState(false);
  const events = w.activity.filter(
      (e) => filter === "all" || e.root_id === root,
    ),
    u = w.upkeep;
  return (
    <div className="page">
      <div className="page-heading">
        <div>
          <p className="eyebrow">THE WORK BENEATH THE WORDS</p>
          <h1>Activity console</h1>
          <p>
            Observe signals, attention, reviews and durable changes as they
            happen.
          </p>
        </div>
        <button
          className="secondary"
          disabled={!u}
          onClick={() => setSettings(true)}
        >
          <SlidersHorizontal size={15} />
          Budget controls
        </button>
      </div>
      <KnowledgePanel root={root} />
      <MeshPanel root={root} />
      <AssemblyPanel root={root} />
      <div className="stats-strip">
        <div>
          <span>Queued signals</span>
          <strong>{u?.index?.queued_signals || 0}</strong>
        </div>
        <div>
          <span>Scheduled reviews</span>
          <strong>{u?.index?.scheduled_reviews || 0}</strong>
        </div>
        <div>
          <span>Review budget used</span>
          <strong>
            {u?.usage?.episodes || 0}
            <small> / {u?.settings?.max_episodes || 0}</small>
          </strong>
        </div>
        <div>
          <span>Background upkeep</span>
          <strong className="text-status">
            {u?.settings.enabled ? "Enabled" : "Paused"}
          </strong>
        </div>
      </div>
      <div className="console-tools">
        <div className="segmented">
          <button
            className={filter === "scope" ? "active" : ""}
            onClick={() => setFilter("scope")}
          >
            This scope
          </button>
          <button
            className={filter === "all" ? "active" : ""}
            onClick={() => setFilter("all")}
          >
            All activity
          </button>
        </div>
        <div>
          <button
            className="secondary"
            disabled={!u}
            onClick={() =>
              w.run(() =>
                w.call("upkeep.configure", "upkeep.configured", {
                  expected_version: u.settings.version,
                  settings: { enabled: !u.settings.enabled },
                }),
              )
            }
          >
            {u?.settings.enabled ? <Pause size={14} /> : <Play size={14} />}{" "}
            {u?.settings.enabled ? "Pause upkeep" : "Enable upkeep"}
          </button>
          <button
            className="primary"
            disabled={!chat}
            onClick={() => setReview(true)}
          >
            <Leaf size={15} />
            Suggest a follow-up
          </button>
        </div>
      </div>
      <div className="event-table live-events">
        <div className="table-head">
          <span>Time</span>
          <span>Event</span>
          <span>Scope</span>
          <span>Trace</span>
        </div>
        {events.map((e) => (
          <button key={e.seq} onClick={() => setSelected(e)}>
            <time>{stamp(e.time_ms)}</time>
            <span
              className={
                "event-type " +
                (e.type.includes("failed") || e.type.includes("rejected")
                  ? "bad"
                  : "")
              }
            >
              <span className="event-dot" />
              {label(e.type)}
            </span>
            <span className="truncate">
              {w.snapshot.projects.find((p) => p.id === e.root_id)?.name ||
                e.root_id}
            </span>
            <code>
              #{e.seq} <ArrowUpRight size={12} />
            </code>
          </button>
        ))}
        {!events.length && (
          <div className="empty-panel">
            <Activity />
            <h3>Quiet is a valid state.</h3>
            <p>
              No work is manufactured to keep this screen moving. Events will
              arrive over the live connection.
            </p>
          </div>
        )}
      </div>
      <p className="footnote">
        Showing the latest {events.length} received events. Soul status →
        Provenance provides persisted history. No polling.
      </p>
      <section className="panel">
        <h3>
          <Clock size={16} />
          Reviews & held work
        </h3>
        {(u?.reviews || [])
          .filter((r) => filter === "all" || r.root_id === root)
          .slice(-20)
          .map((r) => (
            <div className="review-row" key={r.key}>
              <span>
                <b>{label(r.status)}</b>
                <small>{r.reason}</small>
              </span>
              <button className="text-button" onClick={() => setSelected(r)}>
                Inspect <ArrowUpRight size={13} />
              </button>
              {["resting", "awaiting_budget", "awaiting_source"].includes(
                r.status,
              ) && (
                <button
                  className="secondary"
                  onClick={() =>
                    w.run(() =>
                      w.call("review.retry", "review.changed", {
                        root_id: r.root_id,
                        review_key: r.key,
                        expected_version: r.version,
                      }),
                    )
                  }
                >
                  Retry
                </button>
              )}
            </div>
          ))}
        {!u?.reviews?.length && <p className="muted">No unresolved reviews.</p>}
      </section>
      {selected && (
        <Modal title="Event detail" wide onClose={() => setSelected(null)}>
          <pre className="json-view">{JSON.stringify(selected, null, 2)}</pre>
        </Modal>
      )}
      {settings && (
        <Modal title="Shared upkeep budget" onClose={() => setSettings(false)}>
          <p className="muted">
            These limits apply across projects. Changes are journaled. Excess
            work waits or reports a reason.
          </p>
          <form
            onSubmit={async (e) => {
              e.preventDefault();
              const f = new FormData(e.currentTarget),
                settings = {};
              for (const k of [
                "max_concurrent_upkeep",
                "max_episodes",
                "max_foundation_calls",
                "upkeep_refractory_ms",
              ])
                settings[k] = Number(f.get(k));
              const ok = await w.run(() =>
                w.call("upkeep.configure", "upkeep.configured", {
                  expected_version: u.settings.version,
                  settings,
                }),
              );
              if (ok) setSettings(false);
            }}
          >
            {[
              ["max_concurrent_upkeep", "Concurrent reviews", 1, 4],
              ["max_episodes", "Episodes per window", 0, 100],
              ["max_foundation_calls", "Foundation calls per window", 0, 100],
              [
                "upkeep_refractory_ms",
                "Refractory interval (ms)",
                10,
                604800000,
              ],
            ].map(([key, title, min, max]) => (
              <label key={key}>
                {title}
                <input
                  name={key}
                  type="number"
                  min={min}
                  max={max}
                  defaultValue={u.settings[key]}
                  required
                />
              </label>
            ))}
            <footer>
              <button className="primary">Apply limits</button>
            </footer>
          </form>
        </Modal>
      )}
      {review && (
        <Modal title="A thought for later" onClose={() => setReview(false)}>
          <p className="muted">
            This review may contribute to “{chat.title}” while you’re away.
            Upkeep must be enabled, and quiet contribution limits still apply.
          </p>
          <form
            onSubmit={async (e) => {
              e.preventDefault();
              const f = new FormData(e.currentTarget);
              const ok = await w.run(() =>
                w.call("chat.review", "chat.reviewed", {
                  chat_id: chat.id,
                  text: f.get("text"),
                  due_in_ms: Number(f.get("delay")) * 60000,
                }),
              );
              if (ok) setReview(false);
            }}
          >
            <label>
              What deserves another look?
              <textarea name="text" required maxLength={2048} autoFocus />
            </label>
            <label>
              Review after
              <select name="delay">
                <option value="1">1 minute</option>
                <option value="15">15 minutes</option>
                <option value="60">1 hour</option>
                <option value="1440">Tomorrow</option>
              </select>
            </label>
            <footer>
              <button className="primary">Save follow-up</button>
            </footer>
          </form>
        </Modal>
      )}
    </div>
  );
}
