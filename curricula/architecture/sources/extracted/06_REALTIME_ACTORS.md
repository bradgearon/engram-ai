# Distilled — Real-Time, Concurrent, and Actor Patterns

## Reactor

Single-threaded (or few-threaded) event loop demultiplexes readiness events and dispatches handlers. Good for high connection counts with light per-request work. Inverts control (“don’t call us, we’ll call you”).

## Proactor

Asynchronous operation completion drives the handlers. Better when the platform supports efficient async I/O and work is heavier.

## Actor model

- Actors have private state, process one message at a time, and communicate only by messages.
- Location transparency: same model from in-process to cross-network.
- Supervision: failure is contained by a parent policy (restart, stop, escalate).
- Useful when you want isolated failure domains and elastic scaling of stateful workers.

## Practical distinctions

- Reactor/Proactor: excellent for I/O multiplexing and protocol handling.
- Actors: excellent for stateful, message-driven domain logic with supervision.
- They compose: a reactor or proactor can sit at the edge and forward messages into an actor system.

## Training signal

Given latency, state, and failure requirements, choose reactor-style, actor-style, or hybrid, and name the supervision or back-pressure boundary.
