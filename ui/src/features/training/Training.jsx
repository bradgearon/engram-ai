import { useState } from "react";
import {
  Wind,
  Play,
  Square,
  ArrowUpRight,
  Check,
  BookOpen,
  ShieldCheck,
  FlaskConical,
} from "lucide-react";
import { useWorkspace } from "../../state/Workspace.jsx";
import { shortKey, label } from "../../lib/format.js";
import Modal from "../../components/Modal.jsx";

export default function Training({ root }) {
  const w = useWorkspace(),
    [domain, setDomain] = useState("music"),
    [activate, setActivate] = useState(null),
    [practice, setPractice] = useState(false),
    [task, setTask] = useState(""),
    [candidate, setCandidate] = useState(""),
    [instance, setInstance] = useState("");
  const job = w.training.job || {},
    running = job.status === "running",
    packages = w.packages.packages.filter(
      (p) => p.manifest.action_domain === domain,
    ),
    curriculum = w.training.curricula.find((c) => c.domain === domain);
  const result = w.activity.find(
    (e) => e.type === "wind.checkpoint" && e.data?.id === instance,
  )?.data;
  return (
    <div className="page">
      <div className="page-heading">
        <div>
          <p className="eyebrow">PRACTICE TAKES ROOT</p>
          <h1>Winds & learning</h1>
          <p>
            Teach a small policy, inspect what it learned, then choose when to
            use it.
          </p>
        </div>
        <span className="pill">
          <ShieldCheck size={14} />
          Explicit activation
        </span>
      </div>
      <div className="domain-tabs">
        {[
          ["music", "Music", "Motifs, cadence & composition"],
          [
            "architecture",
            "Architecture",
            "Structure, boundaries & async design",
          ],
        ].map(([id, title, detail]) => (
          <button
            key={id}
            className={domain === id ? "domain active" : "domain"}
            onClick={() => setDomain(id)}
          >
            <span className="domain-icon">{id === "music" ? "♩" : "⌘"}</span>
            <span>
              <b>{title}</b>
              <small>{detail}</small>
            </span>
            {domain === id && <Check size={17} />}
          </button>
        ))}
      </div>
      <div className="training-overview">
        <section className="panel curriculum">
          <div className="panel-eyebrow">
            <BookOpen size={16} />
            LEARNING PACKAGE
          </div>
          <h2>
            {domain === "music"
              ? "A sense of composition"
              : "Thoughtful architecture"}
          </h2>
          <p>
            Work through the supplied lessons, collect graded attempts, and
            build an ABI-compatible Wind candidate.
          </p>
          <div className="training-steps">
            <span>
              01 <b>Practice</b>
            </span>
            <Chevron />
            <span>
              02 <b>Optimize</b>
            </span>
            <Chevron />
            <span>
              03 <b>Evaluate</b>
            </span>
          </div>
          <div className="package-facts">
            <span>Native C++ optimizer</span>
            <span>Separate validation & locked tests</span>
            <span>Checkpointed progress</span>
          </div>
          <footer>
            <button
              className="primary"
              disabled={running || w.trainingStatus !== "connected"}
              onClick={() =>
                w.run(async () => w.train({ type: "training.start", domain }))
              }
            >
              <Play size={15} />
              Train / resume Wind
            </button>
            <button
              className="secondary"
              disabled={!curriculum?.sample}
              onClick={() => {
                setTask(JSON.stringify(curriculum.sample, null, 2));
                setPractice(true);
              }}
            >
              <FlaskConical size={15} />
              Practice together
            </button>
          </footer>
        </section>
        <section className="panel run-panel">
          <div className="panel-eyebrow">
            <span className={running ? "busy-dot" : "live-dot"} />
            WORKSHOP
          </div>
          <h3>
            {running
              ? "Learning in progress"
              : job.status === "idle"
                ? "Ready when you are"
                : label(job.status)}
          </h3>
          <p>
            {job.domain
              ? `${label(job.domain)} · ${job.run_id?.slice(0, 12) || job.id?.slice(0, 12) || ""}`
              : "No background training has been started."}
          </p>
          {job.error && <div className="inline-error">{job.error}</div>}
          <div className="run-log" aria-live="polite">
            {w.training.events.slice(-5).map((e, i) => (
              <div key={i}>
                <span>{label(e.type)}</span>
                <small>
                  {e.epoch != null
                    ? `Epoch ${e.epoch}`
                    : e.message || e.status || e.domain || ""}
                </small>
              </div>
            ))}
          </div>
          {running && (
            <button
              className="secondary"
              onClick={() =>
                w.run(async () => w.train({ type: "training.cancel" }))
              }
            >
              <Square size={13} />
              Stop & keep checkpoint
            </button>
          )}
          {job.report && (
            <details>
              <summary>Evaluation report</summary>
              <pre className="json-view">
                {JSON.stringify(job.report, null, 2)}
              </pre>
            </details>
          )}
        </section>
      </div>
      <div className="section-heading">
        <h2>Registered Winds</h2>
        <span>
          {packages.length} for {domain}
        </span>
      </div>
      <div className="wind-table">
        <div className="table-head">
          <span>Policy</span>
          <span>Maturity</span>
          <span>Status</span>
          <span>Action</span>
        </div>
        {packages.map((p) => (
          <div className="wind-row" key={p.key}>
            <span>
              <b>{p.manifest.id}</b>
              <small>
                Version {p.manifest.policy_version} ·{" "}
                {p.manifest.checksum_fnv1a64}
              </small>
            </span>
            <span className="pill">{p.manifest.maturity}</span>
            <span>
              {w.packages.active[domain] === p.key ? (
                <span className="active-text">Active</span>
              ) : (
                "Candidate"
              )}
            </span>
            <button
              className="secondary"
              disabled={w.packages.active[domain] === p.key}
              onClick={() => setActivate(p)}
            >
              Activate
            </button>
          </div>
        ))}
        {!packages.length && (
          <div className="empty-panel">
            <Wind />
            <h3>No {domain} candidates yet</h3>
            <p>
              Start a training run to create one. A passing gate will never
              activate it automatically.
            </p>
          </div>
        )}
      </div>
      <p className="footnote">
        Training changes the small Wind policy, not your GGUF. Conversations and
        reflections update scoped memory and response priority.
      </p>
      {activate && (
        <Modal
          title="Activate this candidate?"
          onClose={() => setActivate(null)}
        >
          <p>
            This explicitly changes the active <b>{domain}</b> Wind for future
            episodes.
          </p>
          <dl className="activation-details">
            <dt>Candidate</dt>
            <dd>{shortKey(activate.key)}</dd>
            <dt>Current</dt>
            <dd>{shortKey(w.packages.active[domain])}</dd>
          </dl>
          <p className="muted">
            The coordinator will reject activation while this domain has running
            or paused episodes. Training results alone never make this change.
          </p>
          <footer>
            <button className="secondary" onClick={() => setActivate(null)}>
              Keep current Wind
            </button>
            <button
              className="primary"
              onClick={() =>
                w.run(async () => {
                  await w.call("wind.activate", "wind.activated", {
                    domain,
                    package_key: activate.key,
                    expected_active_key: w.packages.active[domain],
                  });
                  setActivate(null);
                })
              }
            >
              Activate at boundary
            </button>
          </footer>
        </Modal>
      )}
      {practice && (
        <Modal
          title="Practice together"
          wide
          onClose={() => setPractice(false)}
        >
          <p className="muted">
            Edit a training exercise, run an attempt, and inspect the heart’s
            evaluation. This does not activate a candidate.
          </p>
          <label>
            Wind for this attempt
            <select
              value={candidate}
              onChange={(e) => setCandidate(e.target.value)}
            >
              <option value="">Currently active {domain} Wind</option>
              {packages.map((p) => (
                <option key={p.key} value={p.key}>
                  {shortKey(p.key)}
                </option>
              ))}
            </select>
          </label>
          <label>
            Structured exercise
            <textarea
              className="code-editor"
              value={task}
              onChange={(e) => setTask(e.target.value)}
              rows={12}
            />
          </label>
          <button
            className="primary"
            onClick={() =>
              w.run(async () => {
                const r = await w.call("task.run", "seed.accepted", {
                  root_id: root,
                  task: JSON.parse(task),
                  text: `Practice ${domain} with this project`,
                  max_steps: 4,
                  ...(candidate ? { package_key: candidate } : {}),
                });
                setInstance(r.data.instance_id);
              })
            }
          >
            <Play size={15} />
            Run practice
          </button>
          {instance && (
            <div className="practice-result">
              <h3>
                {result?.status || "Running"} · {instance}
              </h3>
              {result && (
                <pre className="json-view">
                  {JSON.stringify(
                    {
                      artifact: result.artifact,
                      evaluation: result.evaluation,
                      error: result.error,
                    },
                    null,
                    2,
                  )}
                </pre>
              )}
            </div>
          )}
        </Modal>
      )}
    </div>
  );
}
function Chevron() {
  return <span className="muted">→</span>;
}
