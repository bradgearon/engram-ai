# WDS workspace — v0.5

A local React interface for conversations, souls, Wind learning and inspection. Components are modern JavaScript/JSX with default exports. React 19.3.1, Vite 8.3.6 and the dependency lockfile are pinned for reproducible installation. No TypeScript, Boost, Python numerical training, polling, external fonts or hosted database is required.

## Start everything

From the extracted `wds-ai` folder, configure your existing Six Stars GGUF once:

```powershell
python tools/configure_sixstars.py --six-stars "C:\Projects\the-six-stars"
python tools/workshop.py --config configs/sixstars.local.json start --ui --upkeep
```

Open **http://127.0.0.1:8787**. The first command reads Six Stars' authoritative model manifest and checks the file. The second builds the C++ runtime when needed, starts it, starts the material worker, and serves the production React bundle. Keep that terminal open. Close it with Ctrl+C to stop the supervised processes. A matching prebuilt UI bundle is included; rebuilding edited UI source requires Node.js 22.12+ and npm.

For the deterministic demo, without a GGUF:

```sh
python tools/workshop.py --config configs/offline.json start --ui --upkeep
```

The demo is conspicuously labeled. It is for exercising the controls and persistence, not genuine model conversation. To also begin both training packages at startup:

```sh
python tools/workshop.py --config configs/sixstars.local.json start --ui --upkeep --train music,architecture --resume
```

Do not start a second supervisor against the same data directory or port. `--ui-port 8788` changes the interface port; the runtime port comes from the selected config. The native runtime and interface bind to loopback.

## Conversations and projects

The first connection creates a local account scope and a Personal project. Use **+** beside Projects to create another project. Account preferences can be read through the existing explicit scope grant. Projects keep their own memories and known souls.

Start a conversation and send a message. The coordinator journals both user turns and assistant replies. Responses continue if the browser closes; reconnect and reopen the chat to see the result. The last selected chat and project are remembered on that browser. Earlier messages load on demand.

Choose **AI host** for a foundation response with Wind movement frozen, or **Let a Wind speak** for the selected general Wind's movement. Choose a particular known soul or leave selection automatic. The engram layer retains final speaker selection when a saved, relevant idea warrants it. The result identifies the actual speaker, policy and source. The detail rail always shows the current selection on desktop; the focus controls remain available on smaller screens.

Action Winds for music and architecture produce structured repairs, so they belong in **Winds & learning**, not the free-text policy picker. Selecting a package for an attempt does not activate it globally.

Conversation settings let you rename a chat or move it into a project. Moving preserves the authorship and project attribution of old messages, clears the old continuation link and guest invitations, and uses the destination scope for subsequent requests. Wait for an active reply to finish or cancel it before moving.

To hear from another project, explicitly invite its voice in Conversation settings. The next message goes to that project and is saved in its memory; the entire destination transcript is not copied. Replies carry a visible source-project label. Retrieved feedback from a different conversation is marked as such. Ordinary incidental name mentions do not grant cross-project access.

**Reflect on this** records attributed feedback linked to that attempt. It can affect later retrieval and bounded response priority. It does not fine-tune the GGUF or silently activate a Wind.

## Quiet contributions while away

Each chat can allow or pause quiet contributions. With upkeep enabled, an explicit reflection can create one bounded review if that chat does not already have a scheduled or running review. You can also suggest a timed follow-up from Activity. The neuron still decides whether to move, and the existing shared upkeep budgets reserve its cost.

Completed reviews may add a clearly labeled **Quiet contribution** to their original chat, even with no browser connected. At most one such delivery per chat is allowed in 30 minutes. Busy chats, paused contributions, failed reviews and the quiet interval produce an inspectable held event instead of an unmarked interruption. Held results remain in their episode; this version does not automatically redeliver them after the interval. Orange/green unread markers identify new contributions when you return.

No free-running message timer manufactures conversation. The runtime must remain running while you are away. This is local unattended work, not a cloud agent that operates after your computer is off. Restart pauses in-flight episodes; the UI offers Resume or Stop on their original chat.

## Soul status and provenance

Open **My account** for account-level understanding, or select a project and open **Soul status**.

- **Understanding** shows recorded links from memories to known souls/neurons and their latest response. Click nodes for their original fields, actual produced artifact and memory references. A responding state comes from recorded execution, not decorative simulated activity.
- **Souls** creates or edits declared identities, form, presence, gender information and interests. People, birds, rocks and organizations use the same profile. Test an evidence impulse or inspect and adjust an identity's bounded attention weight.
- **Memories** pages through engrams and attributed astroengram reflections. Inspect attribution, source chat, packet identity, version, uncertainty, reliability and learning links. Revise a memory through a versioned RNA packet, or use it as evidence for a saved speaking idea.
- **Provenance** pages backward through the authoritative hash-linked journal. Original sequence, timestamp, actor, trace ID, previous hash, event hash and data remain inspectable. A per-root byte-offset index fetches only the requested records; the browser never loads the whole chain.

The map displays up to five memory and soul nodes for clarity. The Memories and Provenance tabs expose the rest through explicit pagination. A link means the indicated scoped identity or recorded attribution; it is not proof that every displayed memory participated in every response. Response records contain the exact selected memory references.

These software roles do not claim biological neurons or real gender inference. Unknown identity details remain unspecified.

## Winds and interactive learning

Select Music or Architecture in **Winds & learning**. **Train / resume Wind** runs the existing native optimizer and curriculum pipeline over WebSockets. Progress is pushed; disconnecting the browser does not stop the job. One supervised training run is allowed at a time. Stop retains checkpoints. A stopped supervisor records an interrupted run for explicit resume.

**Practice together** exposes an exercise from the training split. Edit its structured data, pick a registered compatible policy for that attempt, run it, and inspect the heart result. Locked-test examples are not exposed as practice inputs.

Candidates appear in Registered Winds. **Activate** opens a separate explicit confirmation showing the current and proposed policy. The coordinator enforces the episode boundary and expected active key. A passing gate never activates the package. Pause upkeep and finish/cancel paused or running domain episodes if activation reports that the boundary is busy.

## Activity console

Switch between current-scope and all-runtime events. Inspect attention selection, Wind state movement, heart evaluations, RNA acceptance, reflection, queued/held impulses and review outcomes. Only the most recent 200 received events are retained in the live browser view; persisted history is in Soul status.

Budget controls change shared upkeep concurrency, episode and foundation-call limits and refractory time through the coordinator. Pause/resume upkeep, retry an eligible review, or request a follow-up for the selected chat. Evidence test impulses may be held if irrelevant; this is a visible decision, not a failure to execute the UI command.

## Boundaries and failure behavior

- This release is a **trusted, single-operator local application**, not an Internet-exposed multi-tenant service. Account/project scopes are data organization, not authentication. The loopback UI server checks Host and WebSocket Origin and provides a restrictive CSP. Add real authentication and authorization before any remote deployment; do not publish the raw runtime port.
- Chat input is bounded to 2048 UTF-8 bytes. Model-output exhaustion is shown as incomplete with saved partial text. UI acknowledgment timeout says that work may still be running; it never blindly retries mutations.
- The runtime remains the sole durable garden writer. The thin Python server serves static assets, relays WebSockets and supervises native training; its own job metadata is separate from the garden.
- No microphone/TTS or Six Stars UI bridge is included. The exact GGUF was not supplied here, so validation used the explicit demo and test foundations. Windows/macOS execution and real GPU inference still require checks on the target machine.
- Earlier limits remain: deterministic lexical speaker matching, one compute worker, scoped memory-key scans, no typed inflammation workflow or global multi-neuron attention batching.

The new message contract is documented in [WORKSPACE_PROTOCOL.md](WORKSPACE_PROTOCOL.md).

## Development layout

| Path | Responsibility |
| --- | --- |
| `ui/src/App.jsx` | Shell, navigation and project/chat selection |
| `ui/src/features/chat` | Conversation, focus controls, attribution and feedback |
| `ui/src/features/souls` | Status map, profiles, memory and provenance inspection |
| `ui/src/features/training` | Curriculum runs, practice and explicit activation |
| `ui/src/features/activity` | Live events and bounded upkeep controls |
| `ui/src/state/Workspace.jsx` | Event-driven shared state and snapshot recovery |
| `ui/src/lib/client.js` | Correlated WebSocket requests, visible errors, no mutation replay |
| `src/workspace.cpp` | Journaled chat/project commands, contribution delivery and inspection |
| `tools/interface.py` | Loopback static server, WebSocket relay and training supervision |

```sh
cd ui
npm ci
npm run dev
npm test
npm run build
```

For development, run the runtime and `python tools/interface.py --config configs/sixstars.local.json --dev` separately. Vite proxies WebSocket traffic to port 8787. `--dev` relaxes the Origin check for the Vite proxy; never use it for production. An optional ignored `configs/ui.local.json` enables the Vite development harness to launch a runtime itself; omit that file if already running one. Production startup uses the strict interface server and the built static bundle.

The repository ignore rules cover C++/CMake, React/Vite, Python, local configs, model weights, generated runs and test output. The lockfile, source and tests are tracked. No node_modules, model weights, conversation data or machine-specific configuration belong in a release source archive.
