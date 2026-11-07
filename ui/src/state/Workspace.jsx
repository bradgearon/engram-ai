import trace from "../lib/diagnostics.js";
import { createContext, useContext, useEffect, useRef, useState } from "react";
import RuntimeClient from "../lib/client.js";
const Context = createContext(null);
export function useWorkspace() {
  return useContext(Context);
}
export default function Workspace({ children }) {
  const [status, setStatus] = useState("connecting"),
    [error, setError] = useState(""),
    [snapshot, setSnapshot] = useState({ projects: [], chats: [] });
  const [packages, setPackages] = useState({ packages: [], active: {} }),
    [activity, setActivity] = useState([]),
    [mesh, setMesh] = useState(null),
    [upkeep, setUpkeep] = useState(null);
  const [messages, setMessages] = useState({}),
    [streams, setStreams] = useState({}),
    [streamSources, setStreamSources] = useState({}),
    [training, setTraining] = useState({
      job: { status: "idle" },
      events: [],
      curricula: [],
    });
  const [trainingStatus, setTrainingStatus] = useState("connecting"),
    [revision, setRevision] = useState(0);
  const api = useRef(),
    trainer = useRef(),
    refreshing = useRef(false),
    dirty = useRef(false);
  const run = async (action) => {
    try {
      return await action();
    } catch (e) {
      trace("workspace.action_failed", { error: e }, true);
      setError(e.message);
      return null;
    }
  };
  async function refresh() {
    if (refreshing.current) {
      dirty.current = true;
      return;
    }
    refreshing.current = true;
    try {
      const [w, p, u, n] = await Promise.all([
        api.current.call("workspace.snapshot", "workspace.snapshot"),
        api.current.call("package.list", "package.snapshot"),
        api.current.call("upkeep.inspect", "upkeep.snapshot"),
        api.current.call("mesh.inspect", "mesh.snapshot", { summary: true }),
      ]);
      setSnapshot(w.data);
      setPackages(p.data);
      setUpkeep(u.data);
      setMesh(n.data);
      setRevision((r) => r + 1);
    } catch (e) {
      trace("workspace.action_failed", { error: e }, true);
      setError(e.message);
    } finally {
      refreshing.current = false;
      if (dirty.current) {
        dirty.current = false;
        refresh();
      }
    }
  }
  async function initialize() {
    try {
      const result = await api.current.call(
        "workspace.snapshot",
        "workspace.snapshot",
      );
      if (!result.data.projects.length) {
        await api.current.call("project.create", "project.saved", {
          project_id: "account",
          name: "My account",
          purpose: "Shared preferences and personal understanding",
          kind: "account",
        });
        await api.current.call("scope.bind", "scope.bound", {
          root_id: "account",
          kind: "account",
          expected_version: 0,
        });
        await api.current.call("project.create", "project.saved", {
          project_id: "home",
          name: "Personal",
          purpose: "Conversation, ideas and everyday understanding",
        });
        await api.current.call("scope.bind", "scope.bound", {
          root_id: "home",
          kind: "project",
          account_root: "account",
          expected_version: 0,
        });
      }
      await api.current.call("upkeep.watch", "upkeep.snapshot");
      await refresh();
    } catch (e) {
      trace("workspace.action_failed", { error: e }, true);
      setError(e.message);
    }
  }
  useEffect(() => {
    api.current = new RuntimeClient(
      "/runtime",
      (event) => {
        if (event.type === "hello") {
          setSnapshot((s) => ({ ...s, foundation: event.foundation }));
          initialize();
          return;
        }
        if (event.type === "error") {
          setError(event.message);
          if (event.instance_id) refresh();
          return;
        }
        if (event.type === "token") {
          if(event.source_kind) setStreamSources((s) => ({...s,[event.instance_id]:event.source_kind}));
          setStreams((s) => ({
            ...s,
            [event.instance_id]: (s[event.instance_id] || "") + event.text,
          }));
          return;
        }
        if (
          event.type === "wind.checkpoint" &&
          ["paused", "cancelled"].includes(event.data?.status)
        )
          refresh();
        if (event.seq)
          setActivity((a) =>
            [event, ...a.filter((x) => x.seq !== event.seq)].slice(0, 200),
          );
        const w = event.data?.workspace;
        if (w?.chat)
          setSnapshot((s) => ({
            ...s,
            chats: [...s.chats.filter((c) => c.id !== w.chat.id), w.chat],
          }));
        if (w?.project)
          setSnapshot((s) => ({
            ...s,
            projects: [
              ...s.projects.filter((p) => p.id !== w.project.id),
              w.project,
            ],
          }));
        if (w?.message) {
          setMessages((s) => {
            const rows = s[w.message.chat_id] || [];
            return { ...s, [w.message.chat_id]: rows.some((m) => m.id === w.message.id)
              ? rows.map((m) => m.id === w.message.id ? w.message : m) : [...rows, w.message] };
          });
          if (w.message.role === "assistant" && w.message.instance_id)
            setStreams((s) => { const next = { ...s }; delete next[w.message.instance_id]; return next; });
        }
        if (
          [
            "cognition.committed",
            "relationship.declared",
            "speaker.profiled",
            "persona.configured",
            "scope.bound",
            "upkeep.configured",
            "review.suggested",
            "review.finished",
            "reflection.recorded",
            "wind.activated",
            "package.registered",
            "neuron.completed",
            "neuron.failed",
            "material.received",
            "engram.idea_saved",
            "review.changed",
          ].includes(event.type) || (event.seq && event.data?.mesh)
        )
          refresh();
      },
      setStatus,
    );
    trainer.current = new RuntimeClient(
      "/workshop",
      (event) => {
        if (event.type === "training.snapshot") setTraining(event);
        else if (event.type === "error") setError(event.message);
        else
          setTraining((s) => ({
            ...s,
            events: [...s.events, event].slice(-200),
          }));
      },
      setTrainingStatus,
    );
    api.current.connect();
    trainer.current.connect();
    return () => {
      api.current.close();
      trainer.current.close();
    };
  }, []);
  async function history(id, before) {
    const r = await api.current.call("chat.history", "chat.history", {
      chat_id: id,
      ...(before != null ? { before } : {}),
    });
    setMessages((s) => {
      const all = [...(s[id] || []), ...r.data.messages];
      return {
        ...s,
        [id]: [...new Map(all.map((m) => [m.id, m])).values()].sort(
          (a, b) => a.time_ms - b.time_ms,
        ),
      };
    });
    return r.data.next_before;
  }
  return (
    <Context
      value={{
        status,
        error,
        setError,
        snapshot,
        packages,
        activity,
        upkeep,
        mesh,
        messages,
        streams,
        streamSources,
        training,
        trainingStatus,
        revision,
        run,
        refresh,
        history,
        call: (...args) => api.current.call(...args),
        train: (message) => trainer.current.send(message),
        reconnect: () => {
          api.current.connect();
          trainer.current.connect();
        },
      }}
    >
      {children}
    </Context>
  );
}
