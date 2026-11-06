# Workspace protocol — v0.5

All runtime commands use the existing WebSocket envelope: `schema: 1`, `type`, and a unique `request_id`. Replies match `trace_id`. Mutation failures are explicit `error` messages. Every durable garden change is appended by the C++ coordinator. The browser-facing gateway exposes `/runtime` and `/workshop` on the same loopback origin; it does not add a garden writer.

| Command | Main fields | Acknowledgment |
| --- | --- | --- |
| `workspace.snapshot` | none | `workspace.snapshot`: projects, chats, foundation, sequence |
| `project.create` | `project_id`, `name`, `purpose`, optional `kind` account/project | `project.saved` |
| `chat.create` | `chat_id`, `project_id`, `title` | `chat.saved` |
| `chat.update` | `chat_id`, `expected_version`; title/project/soul/mode/wind/background/guests | `chat.saved` |
| `chat.history` | `chat_id`, optional `before` | `chat.history`: bounded messages, older-page cursor |
| `chat.send` | `chat_id`, `text`, optional invited `source_root` | `chat.message`: durable user turn; response follows asynchronously |
| `chat.reflect` | `chat_id`, `text`, optional `instance_id` | `chat.message`: attributed reflection |
| `chat.review` | `chat_id`, `text`, optional `due_in_ms` | `chat.reviewed`; actual result follows shared upkeep gates |
| `soul.inspect` | `root_id`, optional memory `offset` | `soul.snapshot`: scoped graph projections, profile versions, memories, outputs, next offset |
| `provenance.page` | `root_id`, optional `before`, `limit` 1–40 | `provenance.page`: original chain records, older-page cursor |

Identical workspace mutations with the same request ID return the recorded acknowledgment. Reusing that ID for a different intent is rejected. UI timeout does not prove a command failed; reconnect and inspect saved state before repeating. The client never automatically resends a mutation.

`chat.message` events retain original project, source project, source chat, actual speaker/profile, package key, episode status, heart evaluation and selected memory references when present. Moving a chat changes its future scope; it does not rewrite past message authorship. A source project must be explicitly invited before a foreign request is accepted.

A completed review may emit a `chat.message` of kind `contribution`. Disabled contributions, a busy chat, incomplete outcome, capacity, or the 30-minute per-chat interval emit `chat.contribution_held`. The episode artifact remains available; held contributions are not automatically re-delivered in this version.

The `/workshop` socket accepts `training.start` with domain `music` or `architecture`, and `training.cancel`. It publishes `training.snapshot` and existing native workshop progress events. One job runs at a time; process metadata/checkpoints survive a browser disconnect. No workshop message activates a package. The UI uses the existing separate `wind.activate` runtime command, expected active key and episode-boundary guard.

Existing `relationship.declare`, `speaker.profile`, `memory.upsert`, `engram.suggest`, `soul.weight`, `upkeep.configure`, `neuron.signal` and review commands retain their version checks and bounds. See PROTOCOL.md and UPKEEP.md for those contracts.
