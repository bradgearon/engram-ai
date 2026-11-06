# v0.5.0 — the local workspace

This is a new release of the existing project, not a relabeled v0.4 archive. The archive includes the complete previous source, the integrated interface, a matching production UI bundle, new tests, and startup instructions. It excludes build trees, node_modules, machine-specific config, model weights and test conversations.

## Added

- Modern React JavaScript/JSX interface with default-exported components, organized by conversation, soul inspection, learning and activity features.
- Durable project/chat records and attributed messages in the authoritative coordinator journal; restart/history recovery and request-id deduplication.
- AI-host versus general-Wind mode, focused soul and Wind details, explicit guest-project invitations, and visible source attribution.
- Attributed reflection, original response memory references, saved speaking ideas, profile editing, bounded attention weights, and versioned memory revisions.
- Account/project soul status with a selectable memory/identity/response map, paged engrams and astroengrams, and exact paged provenance/RNA records.
- Event-driven activity console with upkeep limits, pause/resume and scheduled follow-ups. Eligible review results can arrive in the originating chat without a browser connected.
- Native training supervision, structured practice, live progress, saved checkpoints, and a separate explicit candidate-activation action.
- Loopback static/WebSocket gateway and one-command startup with `--ui`. UI clients do not directly write garden data or train numerically.
- Visible disconnect, timeout, rendering, rejection and incomplete-output states. No blind replay of mutations.
- Ignore rules for C++/CMake, React/Vite, Python, local configs and generated outputs.

## Upgrade

1. Stop the earlier supervisor gracefully.
2. Extract this release into a new folder. Preserve the old release and its data as a backup.
3. Copy your existing local config into the new `configs/` directory. Point its `data` and `runs` paths to the existing locations, or copy those directories while stopped. Preserve any active candidate package directory and its configured `package_root` too.
4. Start with `python tools/workshop.py --config configs/sixstars.local.json start --ui --upkeep`.
5. Open `http://127.0.0.1:8787`. New account/project workspace records are created on first connection. Previous runtime roots and learning records are preserved but are not automatically converted into chat transcripts.

The chain reader accepts prior records. New workspace events require this runtime; do not run an older writer on the upgraded data. A Wind candidate remains checksummed `policy.wind` plus manifest, using the same bounds and maturity values. UI training and passing gates never change the active Wind.

See [INTERFACE.md](INTERFACE.md) for full use and setup, and [VALIDATION.md](VALIDATION.md) for tests and practical limits.
