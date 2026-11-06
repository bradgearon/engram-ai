import { useEffect, useRef, useState } from "react";
import {
  Plus,
  Network,
  ArrowUpRight,
  ChevronRight,
  Database,
  GitBranch,
  Leaf,
  RefreshCw,
} from "lucide-react";
import { useWorkspace } from "../../state/Workspace.jsx";
import Modal from "../../components/Modal.jsx";
import { stamp, label } from "../../lib/format.js";

export default function SoulStatus({ root }) {
  const w = useWorkspace(),
    [data, setData] = useState(null),
    [events, setEvents] = useState([]),
    [before, setBefore] = useState(null),
    [selected, setSelected] = useState(null),
    [edit, setEdit] = useState(null),
    [tab, setTab] = useState("understanding"),
    [query, setQuery] = useState(""),
    [control, setControl] = useState(null);
  const request = useRef(0);
  async function load() {
    const generation = ++request.current;
    const [s, p] = await Promise.all([
      w.call("soul.inspect", "soul.snapshot", { root_id: root }),
      w.call("provenance.page", "provenance.page", { root_id: root }),
    ]);
    if (generation !== request.current) return;
    setData(s.data);
    setEvents(p.data.events);
    setBefore(p.data.next_before);
  }
  useEffect(() => {
    setSelected(null);
    setData(null);
    if (w.status === "connected") w.run(load);
    return () => {
      request.current++;
    };
  }, [root, w.status]);
  useEffect(() => {
    if (data && w.status === "connected") w.run(load);
  }, [w.revision]);
  const recent = w.activity.filter((e) => e.root_id === root),
    allEvents = [
      ...new Map([...recent, ...events].map((e) => [e.seq, e])).values(),
    ].sort((a, b) => b.seq - a.seq);
  const memories = (data?.memories || []).filter((m) =>
    (m.text + " " + m.source).toLowerCase().includes(query.toLowerCase()),
  );
  const souls = data?.scope?.souls || [];
  async function applyControl(e) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const ok = await w.run(async () => {
      if (control.kind === "memory")
        await w.call("memory.upsert", "memory.accepted", {
          packet: {
            schema: 1,
            intent: "memory.upsert",
            hops: 0,
            root_id: root,
            engram_id: control.record.local_id,
            packet_id: "ui-" + crypto.randomUUID(),
            expected_version: control.record.version,
            expires_ms: Date.now() + 60000,
            text: f.get("text"),
            source: f.get("source"),
            claim_status: f.get("claim"),
          },
        });
      if (control.kind === "idea")
        await w.call("engram.suggest", "engram.idea_saved", {
          root_id: root,
          engram_id: control.record.local_id,
          speaker_soul_id: f.get("soul"),
          topic: f.get("topic"),
          reason: f.get("reason"),
          priority: Number(f.get("priority")),
        });
      if (control.kind === "weight")
        await w.call("soul.weight", "soul.weighted", {
          root_id: root,
          item_key: control.record.key,
          expected_version: control.record.version,
          weight: Number(f.get("weight")),
        });
      if (control.kind === "signal")
        await w.call("neuron.signal", "neuron.decision", {
          root_id: root,
          soul_id: control.record.soul_id,
          text: f.get("text"),
          kind: "evidence",
          max_steps: 1,
        });
      return true;
    });
    if (ok) {
      setControl(null);
      await load();
    }
  }
  async function saveSoul(e) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    await w.run(async () => {
      const id = edit.soul_id || "soul-" + crypto.randomUUID();
      if (id !== root)
        await w.call("relationship.declare", "relationship.declared", {
          root_id: root,
          soul_id: id,
          soul_name: f.get("name"),
          purpose: f.get("purpose"),
          relationship_to: root,
          relationship: "project companion",
          voice_id: "purpose",
          expected_version: edit.version || 0,
          paused: f.get("paused") === "on",
        });
      await w.call("speaker.profile", "speaker.profiled", {
        root_id: root,
        soul_id: id,
        expected_version: data.profile_versions[id] || 0,
        form: f.get("form"),
        gender_type: f.get("gender"),
        presence: f
          .get("presence")
          .split(",")
          .map((x) => x.trim())
          .filter(Boolean),
        interests: f
          .get("interests")
          .split(",")
          .map((x) => x.trim())
          .filter(Boolean),
      });
      setEdit(null);
      await load();
    });
  }
  return (
    <div className="page soul-page">
      <div className="page-heading">
        <div>
          <p className="eyebrow">ROOTED UNDERSTANDING</p>
          <h1>
            {w.snapshot.projects.find((p) => p.id === root)?.name || "Soul"}{" "}
            <span> / soul status</span>
          </h1>
          <p>
            See what is remembered, what responds, and how understanding is
            built.
          </p>
        </div>
        <button className="secondary" onClick={() => w.run(load)}>
          <RefreshCw size={15} />
          Refresh snapshot
        </button>
      </div>
      <div className="stats-strip">
        <div>
          <span>Known souls</span>
          <strong>{souls.length}</strong>
        </div>
        <div>
          <span>Engrams & memories</span>
          <strong>{data?.memory_total || 0}</strong>
        </div>
        <div>
          <span>Responding neurons</span>
          <strong>
            {data?.neurons.filter((n) => n.active).length || 0}
            <small> / {data?.neurons.length || 0}</small>
          </strong>
        </div>
        <div>
          <span>Deferred ideas</span>
          <strong>
            {data?.ideas.filter((i) => i.status === "pending").length || 0}
          </strong>
        </div>
      </div>
      <div className="tabs">
        {["understanding", "souls", "memories", "provenance"].map((t) => (
          <button
            key={t}
            className={tab === t ? "active" : ""}
            onClick={() => setTab(t)}
          >
            {label(t)}
          </button>
        ))}
      </div>
      {tab === "understanding" && (
        <>
          <div className="graph-card">
            <header>
              <div>
                <Network size={17} />
                <h3>What is connected</h3>
              </div>
              <span className="muted small-text">
                Recorded links · select a node to inspect
              </span>
            </header>
            <SoulGraph root={root} data={data} onSelect={setSelected} />
            <footer>
              <span>
                <i className="legend memory" />
                Memory
              </span>
              <span>
                <i className="legend neuron" />
                Neuron / soul
              </span>
              <span>
                <i className="legend output" />
                Response
              </span>
              <small>
                Lines represent recorded attribution or episode links, never
                inferred biological activity.
              </small>
            </footer>
          </div>
          <div className="two-columns">
            <section className="panel">
              <h3>
                <Leaf size={17} />
                Flesh & desire
              </h3>
              <p className="muted">
                Small, event-driven signals that guide review.
              </p>
              {Object.entries(data?.flesh || {})
                .filter(([, v]) => typeof v === "number")
                .map(([k, v]) => (
                  <div className="meter-row" key={k}>
                    <span>{label(k)}</span>
                    <meter min="0" max="1" value={v} />
                    <b>{Math.round(v * 100)}%</b>
                  </div>
                ))}
            </section>
            <section className="panel">
              <h3>
                <GitBranch size={17} />
                Recent formation
              </h3>
              {allEvents.slice(0, 5).map((e) => (
                <button
                  className="event-line"
                  key={e.seq}
                  onClick={() => setSelected(e)}
                >
                  <span className="event-dot" />
                  <span>
                    {label(e.type)}
                    <small>
                      #{e.seq} · {stamp(e.time_ms)}
                    </small>
                  </span>
                  <ChevronRight size={14} />
                </button>
              ))}
              {!allEvents.length && (
                <p className="muted">
                  Events appear as this soul receives attention.
                </p>
              )}
            </section>
          </div>
        </>
      )}
      {tab === "souls" && (
        <>
          <div className="list-toolbar">
            <p>{souls.length} identities in this scope</p>
            <button className="primary" onClick={() => setEdit({})}>
              <Plus size={16} />
              Add soul
            </button>
          </div>
          <div className="soul-cards">
            {souls.map((s) => (
              <section className="soul-card" key={s.soul_id}>
                <span className="soul-symbol small">✳</span>
                <div>
                  <h3>{s.name}</h3>
                  <span className="muted small-text">
                    {s.expression?.form || "Unspecified"} ·{" "}
                    {s.expression?.gender_type || "unspecified"}
                  </span>
                </div>
                <p>{s.purpose}</p>
                <div className="tags">
                  {s.expression?.presence?.map((t) => (
                    <span key={t}>{t}</span>
                  ))}
                </div>
                <footer>
                  <button
                    className="text-button"
                    onClick={() => setSelected(s)}
                  >
                    Inspect <ArrowUpRight size={13} />
                  </button>
                  <button
                    className="text-button"
                    onClick={() => setControl({ kind: "signal", record: s })}
                  >
                    Test impulse
                  </button>
                  <button className="secondary" onClick={() => setEdit(s)}>
                    Edit expression
                  </button>
                </footer>
              </section>
            ))}
          </div>
        </>
      )}
      {tab === "memories" && (
        <>
          <div className="list-toolbar">
            <input
              aria-label="Filter memories"
              placeholder="Search loaded memories…"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
            <span>
              {data?.memory_total || 0} saved · {data?.memories.length || 0}{" "}
              loaded
            </span>
          </div>
          <div className="memory-list">
            {memories.map((m) => (
              <button
                key={m.id}
                className="memory-row"
                onClick={() => setSelected(m)}
              >
                <Database size={18} />
                <span>
                  <b>
                    {m.memory_kind === "reflection"
                      ? "Astroengram · attributed reflection"
                      : "Engram"}
                  </b>
                  <p>{m.text}</p>
                  <small>
                    {m.source} · v{m.version} · {m.claim_status}
                  </small>
                </span>
                <ChevronRight size={16} />
              </button>
            ))}
          </div>
          {data?.next_offset != null && (
            <button
              className="secondary"
              onClick={() =>
                w.run(async () => {
                  const r = await w.call("soul.inspect", "soul.snapshot", {
                    root_id: root,
                    offset: data.next_offset,
                  });
                  setData((s) => ({
                    ...s,
                    memories: [...s.memories, ...r.data.memories],
                    next_offset: r.data.next_offset,
                  }));
                })
              }
            >
              Load more memories
            </button>
          )}
          {!memories.length && (
            <div className="empty-panel">
              <Database />
              <h3>Nothing is hidden here.</h3>
              <p>Conversations and feedback will build the first memories.</p>
            </div>
          )}
        </>
      )}
      {tab === "provenance" && (
        <>
          <div className="list-toolbar">
            <p>Authoritative event chain · newest first</p>
            <span className="pill">Hash-linked journal</span>
          </div>
          <div className="event-table">
            <div className="table-head">
              <span>Sequence</span>
              <span>Event</span>
              <span>Time</span>
              <span>Inspect</span>
            </div>
            {allEvents.map((e) => (
              <button key={e.seq} onClick={() => setSelected(e)}>
                <code>#{e.seq}</code>
                <span>{label(e.type)}</span>
                <time>{stamp(e.time_ms)}</time>
                <ArrowUpRight size={15} />
              </button>
            ))}
          </div>
          {before != null && (
            <button
              className="secondary"
              onClick={() =>
                w.run(async () => {
                  const r = await w.call("provenance.page", "provenance.page", {
                    root_id: root,
                    before,
                  });
                  setEvents((a) => [...a, ...r.data.events]);
                  setBefore(r.data.next_before);
                })
              }
            >
              Go further back
            </button>
          )}
        </>
      )}
      {selected && (
        <Modal
          title="Inspect the record"
          wide
          onClose={() => setSelected(null)}
        >
          <div className="record-summary">
            <span className="pill">
              {selected.type ||
                selected.memory_kind ||
                selected.soul_id ||
                "Recorded state"}
            </span>
            <p>
              {selected.text ||
                selected.purpose ||
                selected.reason ||
                "Original values from the coordinator’s journal and projections."}
            </p>
          </div>
          <pre className="json-view">{JSON.stringify(selected, null, 2)}</pre>
          {selected.local_id && (
            <div className="dialog-actions">
              <button
                className="secondary"
                onClick={() => {
                  setControl({ kind: "memory", record: selected });
                  setSelected(null);
                }}
              >
                Revise memory
              </button>
              <button
                className="secondary"
                onClick={() => {
                  setControl({ kind: "idea", record: selected });
                  setSelected(null);
                }}
              >
                Save a speaking idea
              </button>
            </div>
          )}
          {selected.soul_id &&
            data?.weights?.some(
              (x) =>
                x.key ===
                root +
                  "/" +
                  (selected.soul_id === root ? "root" : "soul") +
                  "/" +
                  selected.soul_id,
            ) && (
              <button
                className="secondary"
                onClick={() => {
                  setControl({
                    kind: "weight",
                    record: data.weights.find(
                      (x) =>
                        x.key ===
                        root +
                          "/" +
                          (selected.soul_id === root ? "root" : "soul") +
                          "/" +
                          selected.soul_id,
                    ),
                  });
                  setSelected(null);
                }}
              >
                Adjust response attention weight
              </button>
            )}
          {selected.local_id && (
            <button
              className="secondary"
              onClick={() => {
                setTab("provenance");
                setSelected(null);
              }}
            >
              Follow the RNA & event chain <ArrowUpRight size={14} />
            </button>
          )}
        </Modal>
      )}
      {control && (
        <Modal
          title={
            {
              memory: "Revise an engram",
              idea: "Save a speaking idea",
              weight: "Response attention",
              signal: "Send a test impulse",
            }[control.kind]
          }
          onClose={() => setControl(null)}
        >
          <form onSubmit={applyControl}>
            {control.kind === "memory" && (
              <>
                <p className="muted">
                  Creates a new version through the coordinator. Earlier RNA and
                  provenance remain in the chain.
                </p>
                <label>
                  Memory
                  <textarea
                    name="text"
                    defaultValue={control.record.text}
                    maxLength={2048}
                    required
                  />
                </label>
                <label>
                  Source
                  <input
                    name="source"
                    defaultValue={control.record.source}
                    maxLength={256}
                    required
                  />
                </label>
                <label>
                  Claim status
                  <select
                    name="claim"
                    defaultValue={control.record.claim_status}
                  >
                    {["hypothetical", "observed", "retired"].map((t) => (
                      <option key={t}>{t}</option>
                    ))}
                  </select>
                </label>
              </>
            )}
            {control.kind === "idea" && (
              <>
                <p className="muted">
                  This evidence-backed idea can choose the outward speaker when
                  its topic becomes relevant. Weak matches stay saved.
                </p>
                <label>
                  Speaker
                  <select name="soul">
                    {souls
                      .filter((s) => s.soul_id !== "workspace-user")
                      .map((s) => (
                        <option key={s.soul_id} value={s.soul_id}>
                          {s.name}
                        </option>
                      ))}
                  </select>
                </label>
                <label>
                  Topic
                  <input name="topic" required maxLength={512} />
                </label>
                <label>
                  Reason
                  <textarea name="reason" required maxLength={512} />
                </label>
                <label>
                  Priority (0–100)
                  <input
                    type="number"
                    name="priority"
                    min="0"
                    max="100"
                    defaultValue="50"
                    required
                  />
                </label>
              </>
            )}
            {control.kind === "weight" && (
              <>
                <p className="muted">
                  A bounded response-priority bias, not truth, authority, or
                  Wind activation.
                </p>
                <label>
                  Weight (0–1)
                  <input
                    type="number"
                    name="weight"
                    min="0"
                    max="1"
                    step="0.01"
                    defaultValue={Number(control.record.weight).toFixed(2)}
                    required
                  />
                </label>
              </>
            )}
            {control.kind === "signal" && (
              <>
                <p className="muted">
                  The neuron decides whether this evidence deserves an impulse.
                  Held or completed work appears in Activity.
                </p>
                <label>
                  Evidence
                  <textarea name="text" maxLength={2048} required />
                </label>
              </>
            )}
            <footer>
              <button className="primary">Apply through coordinator</button>
            </footer>
          </form>
        </Modal>
      )}
      {edit && (
        <Modal
          title={edit.soul_id ? "Edit soul expression" : "Add a soul"}
          onClose={() => setEdit(null)}
        >
          <form onSubmit={saveSoul}>
            <label>
              Name
              <input
                name="name"
                defaultValue={edit.name}
                required
                maxLength={80}
                disabled={edit.soul_id === root}
              />
            </label>
            <label>
              Purpose
              <textarea
                name="purpose"
                defaultValue={edit.purpose}
                required
                maxLength={512}
                readOnly={edit.soul_id === root}
              />
            </label>
            <div className="form-grid">
              <label>
                Form
                <input
                  name="form"
                  defaultValue={edit.expression?.form || "person"}
                  maxLength={80}
                  placeholder="Person, bird, rock, organization"
                />
              </label>
              <label>
                Gender information
                <select
                  name="gender"
                  defaultValue={edit.expression?.gender_type || "unspecified"}
                >
                  {["unspecified", "woman", "man", "nonbinary", "neutral"].map(
                    (v) => (
                      <option key={v}>{v}</option>
                    ),
                  )}
                </select>
              </label>
            </div>
            <label>
              Presence <small>comma-separated</small>
              <input
                name="presence"
                defaultValue={edit.expression?.presence?.join(", ")}
                placeholder="warm, sisterly, curious"
              />
            </label>
            <label>
              Interests <small>comma-separated</small>
              <input
                name="interests"
                defaultValue={edit.expression?.interests?.join(", ")}
                placeholder="music, stories"
              />
            </label>
            {edit.soul_id !== root && (
              <label className="checkbox">
                <input
                  name="paused"
                  type="checkbox"
                  defaultChecked={
                    data?.scope.interests.find(
                      (i) => i.soul_id === edit.soul_id,
                    )?.status === "paused"
                  }
                />
                Keep this soul’s speaking interest paused
              </label>
            )}
            <footer>
              <button className="primary">Save soul</button>
            </footer>
          </form>
        </Modal>
      )}
    </div>
  );
}

function SoulGraph({ root, data, onSelect }) {
  const memory = (data?.memories || []).slice(-5),
    souls = (data?.scope?.souls || [])
      .filter((s) => s.soul_id !== "workspace-user")
      .slice(0, 5),
    neurons = data?.neurons || [];
  const node = (item, x, y, title, sub, kind) => (
    <g
      key={title + sub}
      className={"graph-node " + kind}
      role="button"
      tabIndex={0}
      aria-label={`Inspect ${title}`}
      onClick={() => onSelect(item)}
      onKeyDown={(e) => {
        if (e.key === "Enter" || e.key === " ") onSelect(item);
      }}
    >
      <rect x={x} y={y} width="170" height="50" rx="9" />
      <circle cx={x + 17} cy={y + 25} r="4" />
      <text x={x + 30} y={y + 22}>
        {title.slice(0, 19)}
      </text>
      <text className="node-sub" x={x + 30} y={y + 37}>
        {sub.slice(0, 23)}
      </text>
    </g>
  );
  const height = Math.max(250, 85 + Math.max(memory.length, souls.length) * 68);
  return (
    <svg
      className="soul-graph"
      viewBox={`0 0 800 ${height}`}
      role="group"
      aria-label="Recorded memory, neuron and response connections"
    >
      <text className="graph-label" x="28" y="29">
        MEMORIES / INPUTS
      </text>
      <text className="graph-label" x="315" y="29">
        SOULS / NEURONS
      </text>
      <text className="graph-label" x="600" y="29">
        BUILT RESPONSES
      </text>
      {memory.map((m, i) => {
        const target = m.attribution?.target_soul_id || root,
          j = souls.findIndex((s) => s.soul_id === target);
        return j < 0 ? null : (
          <path
            className="graph-edge"
            key={m.id}
            d={`M198 ${80 + i * 68} C250 ${80 + i * 68},260 ${80 + j * 68},315 ${80 + j * 68}`}
          />
        );
      })}
      {souls.map((s, i) => {
        const n = neurons.find((n) => n.soul_id === s.soul_id);
        return n?.last_instance ? (
          <path
            className={"graph-edge " + (n.active ? "firing" : "")}
            key={s.soul_id}
            d={`M485 ${80 + i * 68} C540 ${80 + i * 68},545 ${80 + i * 68},600 ${80 + i * 68}`}
          />
        ) : null;
      })}
      {memory.map((m, i) =>
        node(
          m,
          28,
          55 + i * 68,
          m.memory_kind === "reflection" ? "Astroengram" : "Engram",
          m.text,
          "memory",
        ),
      )}
      {souls.map((s, i) => {
        const n = neurons.find((n) => n.soul_id === s.soul_id);
        return (
          <g key={s.soul_id}>
            {node(
              { ...s, neuron: n },
              315,
              55 + i * 68,
              s.name,
              n?.active
                ? "Responding now"
                : n
                  ? "Resting · ready"
                  : "Declared · dormant",
              "neuron",
            )}
            {n?.last_instance &&
              node(
                data?.outputs?.find((x) => x.id === n.last_instance) || n,
                600,
                55 + i * 68,
                "Response",
                n.last_instance,
                "output",
              )}
          </g>
        );
      })}
      {!memory.length && (
        <text className="graph-empty" x="28" y="110">
          No memories yet
        </text>
      )}
    </svg>
  );
}
