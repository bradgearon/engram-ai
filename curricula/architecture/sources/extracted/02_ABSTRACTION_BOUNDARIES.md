# Distilled — Abstraction, Boundaries, and Ports & Adapters

## Core idea

Good architecture is mostly the disciplined placement of boundaries so that change in one place does not force change in another.

## Practical rules

- A bounded context owns its language and its model. Crossing the boundary requires an explicit translation (anti-corruption layer, published language, or event contract).
- Ports and adapters (hexagonal): the application core depends only on ports it defines. Driving adapters (UI, tests, CLI) and driven adapters (DB, messaging, external APIs) sit outside. Nothing from the outside reaches into the core except through a port.
- C4 model levels (Context → Container → Component → Code) force you to name the boundary at each zoom level before you fill in details.
- Prefer a smaller number of stable interfaces over many convenient ones.

## Common failures

- Distributed monolith: services that must change and deploy together.
- Leaky abstraction: domain objects or SQL details appearing in API contracts or UI.
- Boundary erosion: “just this once” shared database or shared mutable state.

## Training signal

Given a short system description, the output must name the contexts, the ports, and what is deliberately kept outside each boundary.
