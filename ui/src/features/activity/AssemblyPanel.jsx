import { useEffect, useState } from "react";
import { useWorkspace } from "../../state/Workspace.jsx";
import Modal from "../../components/Modal.jsx";

export default function AssemblyPanel({ root }) {
  const w = useWorkspace();
  const [snapshot, setSnapshot] = useState({ assemblies: [], actions: [] });
  const [selected, setSelected] = useState(null), [teaching, setTeaching] = useState(false);
  useEffect(() => {
    let current = true;
    if (w.status === "connected" && w.snapshot.projects.some((p) => p.id === root))
      w.run(async () => {
        const result = await w.call("assembly.list", "assembly.listed", { root_id: root });
        if (current) setSnapshot(result.data);
      });
    return () => { current = false; };
  }, [root, w.revision, w.status]);
  useEffect(() => { setSelected(null); setTeaching(false); setSnapshot({ assemblies: [], actions: [] }); }, [root]);
  const active = w.packages.packages.find((p) => p.key === w.packages.active.expression);
  return <section className="panel mesh-panel">
    <div className="mesh-heading"><div><p className="eyebrow">FROM CONTRIBUTIONS TO WORDS</p><h2>Engram responses</h2></div>
      <button className="secondary" onClick={() => setTeaching(true)}>Teach a relationship</button></div>
    <p className="muted">Responses gather neuronal contributions and cited evidence before speaking. Current expression: {active?.manifest.action_domain === "expression" ? active.manifest.id : "Native grounded sentences"}.</p>
    <div className="mesh-history">{snapshot.assemblies.map((a) => <button key={a.key} className="mesh-history-row" onClick={() => w.run(async () => {
      const result = await w.call("assembly.inspect", "assembly.snapshot", { root_id: root, assembly_key: a.key }); setSelected(result.data);
    })}><b>{a.status}</b><span>{a.input}</span><small>{a.contributions} contributions · {a.sources} sources · {a.pending} pending</small></button>)}</div>
    {!snapshot.assemblies.length && <p className="muted">Start a neuronal conversation to see its response take shape here.</p>}
    {snapshot.next_before && <p className="footnote">Showing the latest 20 responses. Earlier responses remain in the journal and paged API.</p>}
    <div className="mesh-actions">
      <button className="secondary" disabled={w.trainingStatus !== "connected" || w.training.job?.status === "running"}
        onClick={() => w.train({ type: "training.start", domain: "expression", root_id: root })}>Train expression from feedback</button>
      <label>Activate expression Wind<select value="" onChange={(e) => e.target.value && w.run(() => w.call("wind.activate", "wind.activated", {
        domain: "expression", package_key: e.target.value, expected_active_key: w.packages.active.expression,
      }))}><option value="">Choose a candidate explicitly…</option>{w.packages.packages.filter((p) => p.manifest.action_domain === "expression").map((p) =>
        <option key={p.key} value={p.key}>{p.manifest.id}</option>)}</select></label>
    </div>
    {w.training.job?.domain === "expression" && <p role="status">Expression training: {w.training.job.status}{w.training.job.error && " — " + w.training.job.error}</p>}
    <p className="footnote">Teaching records your statement and source. It does not independently verify the claim or activate a Wind. Expression training learns reply-plan choices; the native renderer preserves the supplied meaning.</p>
    {teaching && <Modal title="Teach or correct a relationship" onClose={() => setTeaching(false)}><form onSubmit={(e) => {
      e.preventDefault(); const data = new FormData(e.currentTarget);
      w.run(async () => { const taught = await w.call("assembly.teach", "assembly.taught", { root_id: root,
        engram_id: data.get("id"), expected_version: Number(data.get("version")), subject: data.get("subject"),
        predicate: data.get("predicate"), object: data.get("object"), source: data.get("source"), exclusive: data.get("exclusive") === "on" });
        if (data.get("importance") !== "") await w.call("assembly.attune", "assembly.attuned", { root_id: root,
          engram_id: data.get("id"), expected_version: taught.data.version, importance: Number(data.get("importance")) });
        setTeaching(false); });
    }}>
      <label>Memory identifier<input name="id" pattern="[A-Za-z0-9_.-]+" required maxLength={80} placeholder="cat-definition" /></label>
      <label>Current version (0 for new)<input name="version" type="number" min="0" defaultValue="0" required /></label>
      <label>Subject<input name="subject" required maxLength={256} placeholder="The Last Cat" /></label>
      <label>Relationship<input name="predicate" defaultValue="is" required maxLength={80} /></label>
      <label>Meaning<textarea name="object" required maxLength={1024} placeholder="an exploration game about missing cats and returning light" /></label>
      <label>Source<input name="source" required maxLength={256} placeholder="My project notes" /></label>
      <label>Attention importance (optional, 0–1)<input name="importance" type="number" min="0" max="1" step="0.05" placeholder="0.5" /></label>
      <label className="check"><input name="exclusive" type="checkbox" />Different values for this relationship should be treated as a conflict</label>
      <button className="primary">Record understanding</button>
    </form></Modal>}
    {selected && <Modal title="Response meaning and provenance" onClose={() => setSelected(null)}>
      <p style={{ whiteSpace: "pre-wrap" }}>{selected.expression?.text || "Still gathering contributions…"}</p>
      {selected.expression?.features && <form onSubmit={(e) => {
        e.preventDefault(); const data = new FormData(e.currentTarget);
        w.run(async () => { await w.call("assembly.feedback", "assembly.feedback_recorded", { root_id: root,
          assembly_key: selected.key, action: data.get("action"), note: data.get("note") }); setSelected(null); });
      }}><label>Expected expression<select name="action" defaultValue={selected.expression.action}>{snapshot.actions.map((a) => <option key={a}>{a}</option>)}</select></label>
        <label>Teaching note<input name="note" maxLength={1024} /></label><button className="primary">Record expression feedback</button></form>}
      <details><summary>Contributions, weights references and source chain</summary><pre className="json-view">{JSON.stringify(selected, null, 2)}</pre></details>
    </Modal>}
  </section>;
}
