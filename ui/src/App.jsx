import trace from "./lib/diagnostics.js";
import { useEffect, useState } from "react";
import {
  MessageSquare,
  Plus,
  Folder,
  Search,
  Network,
  Wind,
  Activity,
  ChevronDown,
  PanelLeftClose,
  PanelLeftOpen,
  ArrowUpRight,
  X,
  RefreshCw,
  UserRound,
  Check,
} from "lucide-react";
import { useWorkspace } from "./state/Workspace.jsx";
import Mark from "./components/Mark.jsx";
import Modal from "./components/Modal.jsx";
import Conversation from "./features/chat/Conversation.jsx";
import SoulStatus from "./features/souls/SoulStatus.jsx";
import Training from "./features/training/Training.jsx";
import ActivityConsole from "./features/activity/ActivityConsole.jsx";

export default function App() {
  const w = useWorkspace();
  const [view, setView] = useState("chat"),
    [root, setRoot] = useState(localStorage.getItem("wds-root") || "home"),
    [chatId, setChatId] = useState(localStorage.getItem("wds-chat") || ""),
    [search, setSearch] = useState(""),
    [modal, setModal] = useState(false),
    [side, setSide] = useState(innerWidth > 680);
  const chat = w.snapshot.chats.find((c) => c.id === chatId),
    project = w.snapshot.projects.find((p) => p.id === root);
  const chats = w.snapshot.chats
    .filter(
      (c) =>
        c.project_id === root &&
        c.title.toLowerCase().includes(search.toLowerCase()),
    )
    .sort((a, b) => b.updated_ms - a.updated_ms);
  useEffect(() => {
    if (chat) setRoot(chat.project_id);
  }, [chat?.project_id]);
  useEffect(() => {
    localStorage.setItem("wds-root", root);
    localStorage.setItem("wds-chat", chatId);
  }, [root, chatId]);
  useEffect(() => {
    trace("workspace.context", { view, root_id: root, chat_id: chatId });
  }, [view, root, chatId]);
  const navigate = (page) => setView(page);
  async function newChat() {
    await w.run(async () => {
      const id = "chat-" + crypto.randomUUID();
      await w.call("chat.create", "chat.saved", {
        chat_id: id,
        project_id: root,
        title: "New conversation",
      });
      setChatId(id);
      setView("chat");
    });
  }
  async function createProject(e) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    await w.run(async () => {
      const id = "project-" + crypto.randomUUID();
      await w.call("project.create", "project.saved", {
        project_id: id,
        name: f.get("name"),
        purpose: f.get("purpose"),
      });
      await w.call("scope.bind", "scope.bound", {
        root_id: id,
        kind: "project",
        account_root: "account",
        expected_version: 0,
      });
      setRoot(id);
      setChatId("");
      setView("chat");
      setModal(false);
    });
  }
  return (
    <div className={`app ${side ? "" : "sidebar-hidden"}`}>
      <a className="skip" href="#main">
        Skip to workspace
      </a>
      <aside className="sidebar">
        <div className="brand">
          <Mark />
          <div>
            <b>WDS</b>
            <span>AI WORKSPACE</span>
          </div>
          <button
            className="icon collapse"
            aria-label="Hide sidebar"
            onClick={() => setSide(false)}
          >
            <PanelLeftClose size={17} />
          </button>
        </div>
        <button
          className="new-chat"
          onClick={newChat}
          disabled={w.status !== "connected" || !project}
        >
          <Plus size={17} />
          New conversation<span>↗</span>
        </button>
        <label className="search">
          <Search size={15} />
          <input
            aria-label="Search conversations"
            placeholder="Search conversations"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        </label>
        <nav aria-label="Workspace navigation">
          {[
            ["chat", "Conversations", MessageSquare],
            ["souls", "Soul status", Network],
            ["training", "Winds & learning", Wind],
            ["activity", "Activity console", Activity],
          ].map(([id, title, Icon]) => (
            <button
              className={view === id ? "nav-item selected" : "nav-item"}
              key={id}
              onClick={() => navigate(id)}
            >
              <Icon size={17} />
              {title}
              {id === "activity" && <span className="live-dot" />}
            </button>
          ))}
        </nav>
        <div className="section-label">
          PROJECTS
          <button
            className="icon"
            aria-label="Create project"
            onClick={() => setModal(true)}
            disabled={w.status !== "connected"}
          >
            <Plus size={15} />
          </button>
        </div>
        <div className="projects">
          {w.snapshot.projects
            .filter((p) => p.kind !== "account")
            .map((p) => (
              <button
                key={p.id}
                className={root === p.id ? "project active" : "project"}
                onClick={() => {
                  setRoot(p.id);
                  setChatId("");
                }}
              >
                <Folder size={16} />
                <span>{p.name}</span>
                {root === p.id && <ChevronDown size={14} />}
              </button>
            ))}
        </div>
        <div className="section-label">
          CONVERSATIONS <span>{chats.length}</span>
        </div>
        <div className="chat-list">
          {chats.map((c) => (
            <button
              key={c.id}
              className={c.id === chatId ? "chat-link active" : "chat-link"}
              onClick={() => {
                setChatId(c.id);
                setView("chat");
              }}
            >
              <span>{c.title}</span>
              {c.pending_signal ? (
                <span className="busy-dot" />
              ) : c.last_background_ms >
                Number(localStorage.getItem("read-" + c.id) || 0) ? (
                <span className="unread-dot" aria-label="New contribution" />
              ) : null}
            </button>
          ))}
          {!chats.length && (
            <p className="muted small-text">
              Your conversations will appear here.
            </p>
          )}
        </div>
        <div className="sidebar-foot">
          <button
            onClick={() => {
              setRoot("account");
              setView("souls");
              setChatId("");
            }}
          >
            <span className="avatar">Y</span>
            <span>
              <b>My account</b>
              <small>Personal soul & memory</small>
            </span>
            <ArrowUpRight size={15} />
          </button>
          <div className="connection">
            <span
              className={w.status === "connected" ? "live-dot" : "offline-dot"}
            />
            {w.status === "connected"
              ? "Local runtime connected"
              : "Runtime " + w.status}
            <span>v0.9.1</span>
          </div>
        </div>
      </aside>
      <main id="main" className="main">
        <header className="topbar">
          <div>
            {!side && (
              <button
                className="icon"
                aria-label="Show sidebar"
                onClick={() => setSide(true)}
              >
                <PanelLeftOpen size={19} />
              </button>
            )}
            <Folder size={16} />
            <span>{project?.name || "Workspace"}</span>
            <span className="slash">/</span>
            <b>
              {view === "chat"
                ? chat?.title || "Conversations"
                : view === "souls"
                  ? "Soul status"
                  : view === "training"
                    ? "Winds & learning"
                    : "Activity console"}
            </b>
          </div>
          <span className="local-label">
            <span className="live-dot" />
            ON YOUR MACHINE
          </span>
        </header>
        {w.status !== "connected" && (
          <div className="notice warning">
            <span>
              Disconnected. Saved conversations remain on the runtime.
            </span>
            <button onClick={w.reconnect}>
              <RefreshCw size={14} />
              Reconnect
            </button>
          </div>
        )}
        {w.snapshot.foundation?.includes("demo") && (
          <div className="demo-banner">
            Local knowledge engine · A GGUF can add language features for
            recognized intents.
          </div>
        )}
        {w.error && (
          <div className="notice error" role="alert">
            <span>{w.error}</span>
            <button
              className="icon"
              aria-label="Dismiss error"
              onClick={() => w.setError("")}
            >
              <X size={16} />
            </button>
          </div>
        )}
        {view === "chat" && (
          <Conversation
            chat={chat}
            root={root}
            onNew={newChat}
            onInspect={() => setView("souls")}
          />
        )}
        {view === "souls" && <SoulStatus root={root} />}
        {view === "training" && <Training root={root} />}
        {view === "activity" && <ActivityConsole root={root} chat={chat} />}
      </main>
      {modal && (
        <Modal title="Create a project" onClose={() => setModal(false)}>
          <p className="muted">
            Give related conversations, souls, and learning a shared home.
          </p>
          <form onSubmit={createProject}>
            <label>
              Project name
              <input
                name="name"
                required
                maxLength={80}
                placeholder="The Last Cat"
                autoFocus
              />
            </label>
            <label>
              Purpose
              <textarea
                name="purpose"
                required
                maxLength={512}
                placeholder="What are we building or understanding here?"
              />
            </label>
            <div className="form-note">
              <Check size={15} />
              Shares account preferences; keeps project memories scoped.
            </div>
            <footer>
              <button
                type="button"
                className="secondary"
                onClick={() => setModal(false)}
              >
                Cancel
              </button>
              <button className="primary">Create project</button>
            </footer>
          </form>
        </Modal>
      )}
    </div>
  );
}
