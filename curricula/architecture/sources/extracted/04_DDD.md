# Distilled — Domain-Driven Design (Tactical + Strategic)

## Strategic

- Bounded Context: explicit boundary around a model and its ubiquitous language.
- Context Map: relationships between contexts (Shared Kernel, Customer/Supplier, Conformist, Anti-Corruption Layer, Separate Ways, Open Host Service, Published Language).
- Subdomain classification (Core, Supporting, Generic) guides investment and team boundaries.

## Tactical building blocks

- Entity: identity that persists across state changes.
- Value Object: defined by its attributes; immutable preferred.
- Aggregate: consistency boundary; external references only to the root.
- Domain Event: something that happened that domain experts care about.
- Repository: illusion of an in-memory collection of aggregates.
- Domain Service: operation that does not naturally belong to an entity or value object.

## Supple design hints

- Intention-revealing interfaces
- Side-effect-free functions where possible
- Assertions and specifications for complex rules

## Training signal

Given a domain description, identify at least one aggregate boundary, the ubiquitous language terms that must not leak, and the integration style to neighboring contexts.
