import { useEffect, useState } from "react";
import { Network, Play, Pause, Plus, ArrowRight, Globe, Download } from "lucide-react";
import { useWorkspace } from "../../state/Workspace.jsx";
import Modal from "../../components/Modal.jsx";

export default function MeshPanel({ root }) {
  const w = useWorkspace();
  const [edit, setEdit] = useState(null), [selected, setSelected] = useState(null);
  const [target, setTarget] = useState(""), [text, setText] = useState("");
  const [scope, setScope] = useState(null);
  useEffect(() => { setTarget(""); setSelected(null); setEdit(null); }, [root]);
  useEffect(() => {
    let current = true;
    if (w.status === "connected" && w.snapshot.projects.some((p) => p.id === root))
      w.run(async () => {
        const result = await w.call("mesh.inspect", "mesh.snapshot", { root_id: root });
        if (current) setScope({ root, data: result.data });
      });
    return () => { current = false; };
  }, [root, w.revision, w.status]);
  const m = scope?.root === root ? scope.data : null;
  if (!m) return <section className="panel">Connecting to the neuronal mesh…</section>;
  const nodes = m.nodes.filter((n) => n.root_id === root);
  const routes = m.routes.filter((r) => r.root_id === root && r.enabled);
  const pulses = m.pulses.filter((p) => p.root_id === root);
  const settings = m.settings;
  const configure = (changes) => w.run(() => w.call("mesh.configure", "mesh.configured", {
    expected_version: settings.version, settings: changes,
  }));
  const saveNode = (node, changes) => w.call("mesh.node.register", "mesh.node_registered", {
    root_id: root, node_id: node.id, soul_id: node.soul_id,
    expected_version: node.version, ...changes,
  });
  async function submit(event) {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const ok = await w.run(async () => {
      if (edit.kind === "node") {
        const existing = edit.node;
        await w.call("mesh.node.register", "mesh.node_registered", {
          root_id: root, node_id: existing?.id || form.get("id"),
          soul_id: existing?.soul_id || root, expected_version: existing?.version || 0,
          purpose: form.get("purpose"), examples: JSON.parse(form.get("examples") || "[]"),
          minimum_strength: Number(form.get("strength")), minimum_margin: Number(form.get("margin")),
        });
      } else {
        const from = form.get("from"), to = form.get("to");
        const old = m.routes.find((r) => r.from === root + "/" + from && r.to === root + "/" + to);
        await w.call("mesh.route.connect", "mesh.route_connected", {
          root_id: root, from, to, expected_version: old?.version || 0, weight: Number(form.get("weight")),
        });
      }
      return true;
    });
    if (ok) setEdit(null);
  }
  const activate = async (event) => {
    const key = event.target.value;
    if (!key) return;
    await w.run(() => w.call("wind.activate", "wind.activated", {
      domain: "neural", package_key: key, expected_active_key: w.packages.active.neural,
    }));
  };
  return <section className="panel mesh-panel">
    <div className="mesh-heading"><div><p className="eyebrow">SMALL DECISIONS, CONNECTED UNDERSTANDING</p>
      <h2><Network size={20} /> Neuronal mesh</h2></div>
      <span className="pill">{settings.enabled ? "Listening" : "Paused"}</span></div>
    <p className="muted">Signals wake relevant neurons. Local decisions run first; unresolved signals can request Qwen representations or attributed web excerpts. Quiet nodes rest.</p>
    <div className="stats-strip">
      <div><span>Impulses this window</span><strong>{m.usage.impulses}<small> / {settings.max_impulses_per_window}</small></strong></div>
      <div><span>Representation budget used</span><strong>{m.usage.knowledge_calls}<small> / {settings.max_knowledge_calls}</small></strong></div>
      <div><span>Waiting / working</span><strong>{m.pending} / {m.running}</strong></div>
      <div><span>Knowledge backend</span><strong className="text-status">{m.representations_available ? "Qwen ready" : "Local only"}</strong></div>
    </div>
    <div className="mesh-actions">
      <button className="secondary" onClick={() => configure({ enabled: !settings.enabled })}>{settings.enabled ? <Pause size={14} /> : <Play size={14} />}{settings.enabled ? "Pause mesh" : "Resume mesh"}</button>
      <button className="secondary" onClick={() => configure({ web_enabled: !settings.web_enabled })}><Globe size={14} />{settings.web_enabled ? "Pause web lookup" : "Enable Wikipedia lookup"}</button>
      <button className="secondary" onClick={() => setEdit({ kind: "node" })}><Plus size={14} />Neuron</button>
      <button className="secondary" disabled={nodes.length < 2} onClick={() => setEdit({ kind: "route" })}><ArrowRight size={14} />Connect</button>
    </div>
    <div className="mesh-map" role="list" aria-label="Neurons and their outgoing routes">
      {nodes.map((n) => <article key={n.key} className={"mesh-node " + (n.active ? "working" : "")} role="listitem">
        <div className="mesh-heading"><strong>{n.id}</strong><span className="pill">{!n.enabled ? "Paused" : n.active ? "Working" : n.last_action}</span></div>
        <p>{n.purpose}</p><small>{n.impulses} impulses · {n.cache_hits} representation cache hits</small>
        <div className="mesh-route-list">{routes.filter((r) => r.from === n.key).map((r) => <span key={r.key}><ArrowRight size={12} />{r.to.split("/").at(-1)}</span>)}</div>
        <div className="mesh-actions"><button className="text-button" onClick={() => setEdit({ kind: "node", node: n })}>Inspect / teach</button>
          <button className="text-button" disabled={!!n.active} onClick={() => w.run(() => saveNode(n, { enabled: !n.enabled }))}>{n.enabled ? "Pause" : "Enable"}</button></div>
      </article>)}
    </div>
    <form className="mesh-signal" onSubmit={(e) => { e.preventDefault(); w.run(async () => {
      await w.call("mesh.signal", "mesh.signal_queued", { root_id: root, node_id: target || nodes[0]?.id, text }); setText("");
    }); }}>
      <label>Target neuron<select value={target || nodes[0]?.id || ""} onChange={(e) => setTarget(e.target.value)}>{nodes.map((n) => <option key={n.key} value={n.id}>{n.id}</option>)}</select></label>
      <label>Signal<input value={text} onChange={(e) => setText(e.target.value)} maxLength={4096} required placeholder="What should this neuron attend to?" /></label>
      <button className="primary" disabled={!nodes.length}><Play size={14} />Send impulse</button>
    </form>
    <div className="mesh-history">
      {pulses.slice(0, 12).map((p) => <button key={p.key} className="mesh-history-row" onClick={() => setSelected(p)}>
        <span>{p.node_key.split("/").at(-1)}</span><b>{p.decision?.action || p.status}</b><span>{p.reason}</span><small>{p.elapsed_ms ?? "—"} ms</small>
      </button>)}
      {!pulses.length && <p className="muted">Add a memory or send a signal to see the mesh respond.</p>}
    </div>
    <div className="mesh-actions"><button className="secondary" disabled={w.training.job?.status === "running" || w.trainingStatus !== "connected"}
      onClick={() => w.train({ type: "training.start", domain: "neural", root_id: root })}>Train a Wind from labeled impulses</button>
      <button className="secondary" onClick={() => w.run(async () => {
        const result = await w.call("mesh.training.export", "mesh.training.rows", { root_id: root });
        const url = URL.createObjectURL(new Blob([JSON.stringify(result.data, null, 2)], { type: "application/json" }));
        const a = document.createElement("a"); a.href = url; a.download = root + "-neural-feedback.json"; a.click(); URL.revokeObjectURL(url);
      })}><Download size={14} />Export feedback</button>
      <label>Activate neural Wind<select value="" onChange={activate}><option value="">Explicitly select a candidate…</option>
        {w.packages.packages.filter((p) => p.manifest.action_domain === "neural").map((p) => <option value={p.key} key={p.key}>{p.manifest.id}</option>)}</select></label>
    </div>
    {w.training.job?.domain === "neural" && <p role="status">Training: {w.training.job.status}{w.training.job.error ? " — " + w.training.job.error : ""}
      {w.training.job.report && " · Locked-test accuracy: " + Math.round(w.training.job.report.accuracy * 100) + "% · Coverage: " + Math.round(w.training.job.report.coverage * 100) + "%"}</p>}
    <p className="footnote">Feedback trains a candidate; it never activates one. Similarity scores are uncalibrated. A decision can remain held. Web lookup sends the signal text, and imported excerpts remain unverified.</p>
    {edit && <Modal title={edit.kind === "node" ? "Neuron & demonstrations" : "Connect neurons"} onClose={() => setEdit(null)}><form onSubmit={submit}>
      {edit.kind === "node" ? <>
        {!edit.node && <label>Identifier<input name="id" required pattern="[A-Za-z0-9_.-]+" maxLength={80} /></label>}
        <label>Purpose<input name="purpose" required maxLength={1024} defaultValue={edit.node?.purpose || ""} /></label>
        <label>Semantic demonstrations<textarea name="examples" rows={7} defaultValue={JSON.stringify(edit.node?.examples || [], null, 2)} /></label>
        <p className="footnote">Optional JSON: [{`{"text":"leave room for reflection","action":"rest"}`}, {`{"text":"share this correction","action":"propagate"}`}]. At least two different action labels. Qwen evaluates representations; it does not generate an answer.</p>
        <label>Minimum strength<input name="strength" type="number" min="0" max="1" step="0.01" defaultValue={edit.node?.minimum_strength ?? 0.7} /></label>
        <label>Minimum margin<input name="margin" type="number" min="0" max="1" step="0.01" defaultValue={edit.node?.minimum_margin ?? 0.1} /></label>
      </> : <>
        <label>From<select name="from">{nodes.map((n) => <option key={n.key}>{n.id}</option>)}</select></label>
        <label>To<select name="to" defaultValue={nodes[1]?.id}>{nodes.map((n) => <option key={n.key}>{n.id}</option>)}</select></label>
        <label>Impulse weight<input name="weight" type="number" min="0" max="1" step="0.05" defaultValue="0.8" /></label>
      </>}
      <button className="primary">Save</button>
    </form></Modal>}
    {selected && <Modal title="Impulse provenance & feedback" onClose={() => setSelected(null)}>
      <p>{selected.reason}</p><pre className="json-view">{JSON.stringify(selected, null, 2)}</pre>
      {selected.decision && !["queued", "running", "waiting_web"].includes(selected.status) && <form onSubmit={(e) => {
        e.preventDefault(); const data = new FormData(e.currentTarget);
        w.run(async () => { await w.call("mesh.feedback", "mesh.feedback_recorded", { root_id: root,
          pulse_key: selected.key, action: data.get("action"), note: data.get("note") }); setSelected(null); });
      }}><label>Expected action<select name="action" defaultValue={selected.decision.action}>{m.actions.map((a) => <option key={a}>{a}</option>)}</select></label>
        <label>What should it learn?<input name="note" maxLength={1024} /></label><button className="primary">Record teaching feedback</button></form>}
      {!["queued", "running", "waiting_web"].includes(selected.status) && <button className="secondary" onClick={() => w.run(async () => {
        await w.call("mesh.pulse.retry", "mesh.pulse_changed", { root_id: root, pulse_key: selected.key }); setSelected(null);
      })}>Retry with current evidence</button>}
    </Modal>}
  </section>;
}
