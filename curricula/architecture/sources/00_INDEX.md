# Software Architecture & Design Learning Package

## Purpose

High-signal text resources and distilled principles for becoming strong at abstraction, design patterns, service-oriented / event-driven architecture, and real-time systems. Built entirely from public internet sources.

Use this the same way as the music package: seed knowledge for practice trajectories, constraint checkers, and small wind-style policies that improve held-out design or code tasks.

## Recommended reading / study order

1. Abstraction and boundaries (C4, ports & adapters, bounded contexts)
2. Classic and enterprise design patterns (GoF summaries + Fowler EAA)
3. Domain-Driven Design tactical + strategic patterns
4. Event-driven and messaging patterns
5. Real-time / concurrent patterns (Reactor, Proactor, Actor model)
6. SOA / microservices trade-offs and anti-patterns

## Core free resources

### Awesome lists and catalogs
- Awesome Software Architecture: https://github.com/simskij/awesome-software-architecture
- Awesome System Design Resources: https://github.com/ashishps1/awesome-system-design-resources
- Awesome Design Patterns: https://github.com/DovAmir/awesome-design-patterns
- Free DDD Learning Resources: https://github.com/ddd-crew/free-ddd-learning-resources

### Patterns and enterprise architecture
- Martin Fowler — Catalog of Patterns of Enterprise Application Architecture: https://martinfowler.com/eaaCatalog/
- Martin Fowler — Domain-Driven Design article: https://martinfowler.com/bliki/DomainDrivenDesign.html
- DDD Starter Modelling Process: https://github.com/ddd-crew/ddd-starter-modelling-process

### Event-driven, messaging, real-time
- arc42 Event-Driven Architecture: https://quality.arc42.org/approaches/event-driven-architecture
- Reactor pattern (Wikipedia): https://en.wikipedia.org/wiki/Reactor_pattern
- Patterns for Distributed Real-time and Embedded Systems (Schmidt / ACE / POSA): https://www.dre.vanderbilt.edu/~schmidt/patterns-ace.html
- InfoQ — Designing Resilient Event-Driven Systems at Scale: https://www.infoq.com/articles/scalable-resilient-event-systems/

### Additional practical
- Software Patterns Lexicon: https://softwarepatternslexicon.com/
- System Design and Architecture (puncsky): https://github.com/puncsky/system-design-and-architecture

## What a good “architecture wind” should improve

- Drawing or describing clean boundaries (what belongs inside vs outside a context)
- Choosing between synchronous request/response, events, and actors for a given constraint set
- Identifying when a pattern is being misapplied (e.g., distributed monolith, chatty services, missing idempotency)
- Producing a short, auditable design note that names the patterns used and the trade-offs accepted

See `01_TRAINING_INSTRUCTIONS.md` for a concrete plan.
