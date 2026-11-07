import { useEffect, useState } from "react";
import { useWorkspace } from "../../state/Workspace.jsx";

export default function KnowledgePanel({ root }) {
  const w = useWorkspace();
  const [state, setState] = useState(null);
  useEffect(() => {
    let current = true;
    setState(null);
    if (w.status === "connected" && w.snapshot.projects.some((p) => p.id === root))
      w.run(async () => {
        const response = await w.call("cognition.inspect", "cognition.snapshot", { root_id: root });
        if (current) setState(response.data);
      });
    return () => { current = false; };
  }, [root, w.revision, w.status]);
  return <section className="panel mesh-panel">
    <div className="mesh-heading"><div><p className="eyebrow">KNOWLEDGE & PRACTICE</p><h2>Learned operations</h2></div>
      <span>Scope version {state?.version ?? "…"}</span></div>
    <p className="muted">Inspect explicitly taught operations and their evidence here. English learning uses conversation sequences in Winds &amp; learning. Optional Qwen assistance generates replies and is labeled as teacher output.</p>
    <div className="mesh-history">{Object.values(state?.actions || {}).map((action) => <details key={action.id} className="panel">
      <summary>{action.id} · {action.kind} · {action.samples.length} observations · v{action.version}{action.ambiguous ? " · conflicting observations" : ""}</summary>
      <p>Hypothesized preconditions: <code>{JSON.stringify(action.preconditions)}</code></p>
      <p>Predicted effects: <code>{JSON.stringify(action.effects)}</code></p>
      <p>Matching predicate edges receive bounded activation. These weights affect response priority, not factual authority.</p>
      <pre className="json-view">{JSON.stringify({ edges: action.edges, observations: action.samples }, null, 2)}</pre>
    </details>)}</div>
    {!Object.keys(state?.actions || {}).length && <p className="muted">Structured operations appear here after explicit teaching. A teacher conversation does not automatically create a verified operation.</p>}
    <details><summary>{Object.keys(state?.entities || {}).length} entities · {state?.language_samples?.length || 0} language examples</summary>
      <pre className="json-view">{JSON.stringify(state?.entities || {}, null, 2)}</pre></details>
    <button className="secondary" disabled={w.trainingStatus !== "connected" || w.training.job?.status === "running"}
      onClick={() => w.train({ type: "training.start", domain: "english", root_id: root })}>Train English sequences</button>
    {w.training.job?.domain === "english" && <p role="status">Language training: {w.training.job.status}{w.training.job.error && " — " + w.training.job.error}</p>}
    <label>Activate a trained Language Wind<select value="" onChange={(e) => e.target.value && w.run(() => w.call("wind.activate", "wind.activated", {
      domain: "language", package_key: e.target.value, expected_active_key: w.packages.active.language,
    }))}><option value="">Choose a candidate explicitly…</option>{w.packages.packages.filter((p) => p.manifest.feature_schema === "language.sequence/1").map((p) =>
      <option key={p.key} value={p.key}>{p.manifest.id}</option>)}</select></label>
    <p className="footnote">Teaching is scoped to this project. Simulation never counts as an observed success. Full decisions and source chains appear in cognition.committed events below.</p>
  </section>;
}
