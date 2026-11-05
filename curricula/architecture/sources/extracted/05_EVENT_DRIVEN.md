# Distilled — Event-Driven Architecture & Messaging

## Core idea

Producers publish immutable facts; consumers subscribe and react. The producer does not know the consumer set.

## Key properties

- Events are past-tense facts (OrderPlaced), not commands.
- Delivery guarantees, ordering, and idempotency are explicit design choices, not afterthoughts.
- At-least-once + idempotent consumer is the common practical default.
- Queues and logs absorb bursts and enable replay.

## Resilience patterns

- Circuit breaker, bulkhead, timeout, retry with backoff + jitter.
- Shuffle sharding or similar isolation so one noisy tenant does not take down the fleet.
- Dead-letter handling and observability of consumer lag.

## When not to use

- Caller needs an immediate, strongly consistent answer in the request path.
- So few participants that a broker adds more moving parts than it removes.

## Training signal

Given a flow, state the event contracts, the delivery guarantee assumed, the idempotency strategy, and one failure mode that is contained.
