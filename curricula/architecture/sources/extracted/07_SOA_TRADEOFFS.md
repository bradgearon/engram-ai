# Distilled — SOA, Services, and Trade-offs

## Service orientation

A service is a capability offered at a stable contract boundary. Consumers depend on the contract, not the implementation.

## Recurring trade-offs

- Sync request/response: simple, higher coupling in time, easier consistency in the caller.
- Async events: looser coupling, eventual consistency, better for fan-out and resilience.
- Service granularity: too fine → chatty and operationally expensive; too coarse → loses independent deployability.
- Data ownership: each service should own its data; shared databases recreate the monolith.

## Anti-patterns to reject

- Distributed monolith (coupled deploy, shared DB, synchronous chains that fail together).
- Chatty interfaces that force many round-trips for one user goal.
- Missing explicit versioning or compatibility strategy on contracts.
- “SOA” that is only a set of shared libraries.

## Training signal

Given a set of capabilities and change rates, propose service boundaries, the communication style between them, and the consistency model accepted at each boundary.
